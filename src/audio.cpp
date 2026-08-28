// audio.cpp
// Implementation of the LEDC-PWM audio engine.
//
// Design summary
// --------------
//  - A 256-entry sine table maps to the full PWM duty range centred on the
//    silent point (DUTY_SILENT = 512).  Values are pre-baked so the ISR
//    only needs one multiply, one mask and one LEDC write per sample.
//  - Phase accumulator is 32 bits.  Per-sample increment = freqHz * 2^32
//    / sampleRate.  This gives us a frequency resolution well below 1 Hz
//    for everything in the C4..C5 range.
//  - We use the LEDC high-speed path because the audio timer and the LEDC
//    hardware divider need to be free of Wi-Fi interactions; C3 has no
//    Wi-Fi dependency on LEDC, but the high-speed path is the safest.
//  - The ISR also does the fade-in / fade-out envelope purely by scaling
//    the sine amplitude by an 8-bit `g_envelope` register (0..255).
//  - All volatile state is touched from one core only (the one running
//    the hardware timer).  audio_setFrequency() pushes new requests via
//    a short critical section.

#include "audio.h"
#include "pins.h"
#include "notes.h"

#include <driver/ledc.h>
#include <driver/timer.h>
#include <esp_timer.h>
#include <soc/ledc_periph.h>
#include <soc/timer_group.h>

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Compile-time configuration
// ---------------------------------------------------------------------------
// The ISR fires at 44_100 Hz which gives a Nyquist of 22 kHz - well above
// the ~1.6 kHz analog corner set by R2 + C1.  At this rate the per-sample
// work fits comfortably between interrupts on the RISC-V core.
static constexpr uint32_t kSampleRateHz = 44100;

// Sine table.  Q10 (so values fit in 0..1023 just like the PWM range).
static constexpr int      kSineTableSize = 256;
static int16_t            s_sineTable[kSineTableSize];

// Phase accumulator (32-bit unsigned).
static volatile uint32_t   s_phase = 0;
// Per-sample phase increment.  Recomputed from `s_targetFreq`.
static volatile uint32_t   s_phaseInc = 0;
// Frequency the ISR is currently synthesising (Hz).  0 = silent.
static volatile float      s_currentFreq = 0.0f;
// Frequency requested by the main loop (Hz).  0 = silent.
static volatile float      s_targetFreq  = 0.0f;

// Fade-in / fade-out envelope in 8-bit amplitude (0..255).  Bumped up
// from 0 when a key is pressed, ramped back to 0 after release.
static volatile uint8_t    s_envelope = 0;
// Per-update step for the envelope.  Computed so AUDIO_FADE_IN_MS reaches
// 255 and AUDIO_FADE_OUT_MS reaches 0.
static volatile uint8_t    s_envInStep  = 0;
static volatile uint8_t    s_envOutStep = 0;

// ISR dispatch handle.
static hw_timer_t          *s_audioTimer = NULL;

// ---------------------------------------------------------------------------
// Forward decls
// ---------------------------------------------------------------------------
static void IRAM_ATTR audio_isr(void);
static void             updateEnvelope(void);

// ---------------------------------------------------------------------------
// Sine table generation (called once during init).
// ---------------------------------------------------------------------------
static void buildSineTable() {
    // The PWM range is 0..1023.  We want the centred value (512) to be
    // silent and let the sine swing around it by +/- 511.  To allow
    // per-sample envelope scaling we scale by 256 first, then divide by
    // 255 once the envelope is applied.
    for (int i = 0; i < kSineTableSize; ++i) {
        const float radians = (2.0f * M_PI * i) / kSineTableSize;
        // sine returns -1..+1.  Map to 0..1023 with 512 as the centre.
        const float s = sinf(radians);
        s_sineTable[i] = (int16_t)lroundf(512.0f + 511.0f * s);
    }
}

// ---------------------------------------------------------------------------
// LEDC setup
// ---------------------------------------------------------------------------
static void setupLedc() {
    ledc_timer_config_t timerCfg = {};
    timerCfg.speed_mode = LEDC_LOW_SPEED_MODE;   // C3 LEDC high-speed has
                                                 // only 4 channels and
                                                 // clashes with the touch
                                                 // sensor; use low-speed
                                                 // channel 0 instead.
    timerCfg.duty_resolution = (ledc_timer_bit_t)AUDIO_PWM_RES_BITS;
    timerCfg.timer_num = LEDC_TIMER_0;
    timerCfg.freq_hz = AUDIO_PWM_FREQ_HZ;
    timerCfg.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timerCfg);

    ledc_channel_config_t chCfg = {};
    chCfg.gpio_num = AUDIO_PWM_GPIO;
    chCfg.speed_mode = LEDC_LOW_SPEED_MODE;
    chCfg.channel = (ledc_channel_t)AUDIO_PWM_CHANNEL;
    chCfg.intr_type = LEDC_INTR_DISABLE;
    chCfg.timer_sel = LEDC_TIMER_0;
    chCfg.duty = AUDIO_PWM_DUTY_SILENT;
    chCfg.hpoint = 0;
    ledc_channel_config(&chCfg);

    // Park the output at the silent mid-rail duty so a long silence does
    // not hold the speaker at 0 V (which can cause a small click when the
    // amp wakes up).
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)AUDIO_PWM_CHANNEL,
                  AUDIO_PWM_DUTY_SILENT);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)AUDIO_PWM_CHANNEL);
}

// ---------------------------------------------------------------------------
// Hardware timer setup
// ---------------------------------------------------------------------------
static void setupTimer() {
    // Timer group 0, timer 0 - dedicated to audio.  Divider is set so the
    // counter ticks at 1 MHz; we then ask the hardware to count to
    // 1_000_000 / 44_100 ticks between ISRs.
    s_audioTimer = timerBegin(0, 80, true);  // 80 MHz / 80 = 1 MHz tick
    const uint32_t alarmValue = 1000000UL / kSampleRateHz;  // ~22
    timerAlarmWrite(s_audioTimer, alarmValue, true /* auto-reload */, true /* reload-from-zero */);
    timerAttachInterrupt(s_audioTimer, &audio_isr, true);
    timerAlarmEnable(s_audioTimer);
}

// ---------------------------------------------------------------------------
// Envelope stepper - called from the main loop once per audio update.
// (We keep it light so we can run a 5 ms loop comfortably.)
// ---------------------------------------------------------------------------
static void updateEnvelope() {
    // We treat the envelope like an 8-bit DAC.  When the target frequency
    // is non-zero we ramp up; otherwise we ramp down.
    if (s_targetFreq > 0.0f) {
        // Ramp up.  Use a saturating add to avoid wraparound noise.
        uint16_t next = (uint16_t)s_envelope + (uint16_t)s_envInStep;
        if (next > 255) {
            s_envelope = 255;
        } else {
            s_envelope = (uint8_t)next;
        }
    } else {
        // Ramp down.  Saturating subtract.
        if (s_envelope > s_envOutStep) {
            s_envelope = (uint8_t)(s_envelope - s_envOutStep);
        } else {
            s_envelope = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Audio ISR
// ---------------------------------------------------------------------------
static void IRAM_ATTR audio_isr(void) {
    // Hold the silent duty if there is nothing to play.  The LEDC
    // hardware keeps the channel configured, so writes are cheap.
    if (s_envelope == 0) {
        // Update the phase anyway so we resume in step with the next
        // request - but only if we have a target.
        if (s_currentFreq > 0.0f) {
            s_phase = 0;
            s_currentFreq = 0.0f;
            s_phaseInc = 0;
        }
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)AUDIO_PWM_CHANNEL,
                      AUDIO_PWM_DUTY_SILENT);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)AUDIO_PWM_CHANNEL);
        return;
    }

    // Pick the new sample from the sine table.
    const uint8_t idx = (uint8_t)(s_phase >> 24);   // top 8 bits of phase
    const int16_t s16 = s_sineTable[idx];

    // Apply the envelope:  s = 512 + (s16 - 512) * env / 255
    const int32_t centred = (int32_t)s16 - 512;
    const int32_t scaled  = (centred * (int32_t)s_envelope) / 255;
    int32_t duty = 512 + scaled;
    if (duty < 0)   duty = 0;
    if (duty > AUDIO_PWM_MAX) duty = AUDIO_PWM_MAX;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)AUDIO_PWM_CHANNEL,
                  (uint32_t)duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)AUDIO_PWM_CHANNEL);

    // Advance the phase.  We use a 32-bit accumulator so the per-sample
    // increment stays simple to compute.
    s_phase += s_phaseInc;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void audio_init(void) {
    buildSineTable();
    setupLedc();

    // Envelope step sizes are computed for a 5 ms update rate (200 Hz).
    // The main loop will call updateEnvelope() at that cadence.
    constexpr uint32_t kEnvUpdateHz = 200;
    s_envInStep  = (uint8_t)(255.0f * AUDIO_FADE_IN_MS  / 1000.0f * kEnvUpdateHz);
    s_envOutStep = (uint8_t)(255.0f * AUDIO_FADE_OUT_MS / 1000.0f * kEnvUpdateHz);
    if (s_envInStep == 0)  s_envInStep = 1;
    if (s_envOutStep == 0) s_envOutStep = 1;

    setupTimer();

    s_phase = 0;
    s_phaseInc = 0;
    s_currentFreq = 0.0f;
    s_targetFreq  = 0.0f;
    s_envelope    = 0;
}

void audio_setFrequency(float freqHz) {
    if (freqHz < 0.0f) freqHz = 0.0f;

    // Push the new target to the ISR.  We protect s_targetFreq + s_phaseInc
    // with a critical section so the ISR doesn't observe a torn pair.
    portDISABLE_INTERRUPTS();
    s_targetFreq = freqHz;
    if (freqHz == 0.0f) {
        s_phaseInc = 0;
    } else {
        // phaseInc per sample = freq * 2^32 / sampleRate
        const float inc = freqHz * 4294967296.0f / (float)kSampleRateHz;
        s_phaseInc = (uint32_t)inc;
    }
    // If we are starting a fresh note, also reset the phase so the
    // waveform begins at the zero crossing (avoids a click at boot).
    if (freqHz > 0.0f && s_currentFreq == 0.0f) {
        s_phase = 0;
        s_currentFreq = freqHz;
    }
    if (freqHz == 0.0f) {
        s_currentFreq = 0.0f;
    }
    portENABLE_INTERRUPTS();
}

float audio_getFrequency(void) {
    return s_targetFreq;
}

bool audio_isActive(void) {
    return s_envelope > 0;
}

// ---------------------------------------------------------------------------
// Hook used by main loop to drive the envelope.  Should be called every
// ~5 ms; 200 Hz is a safe cadence.
// ---------------------------------------------------------------------------
void audio_tick(void) __attribute__((weak));
void audio_tick(void) {
    updateEnvelope();
}
