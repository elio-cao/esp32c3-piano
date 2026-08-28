// main.cpp  -  AUDIO STRENGTH / AMP-SD POLARITY TEST build (temporary)
//
// Purpose: determine (a) whether the amplifier/speaker chain produces sound at
// all, and (b) the correct SD enable polarity (active-HIGH vs active-LOW).
//
// Behaviour (loops forever):
//   - A LOUD 440 Hz full-scale square wave is produced on the audio PWM pin
//     (GPIO20) via a bare LEDC duty toggle -- bypassing the synth engine so a
//     weak-amplitude signal cannot be the excuse.
//   - The amp SD pin (GPIO10) is held for 5 seconds at HIGH, then 5 s at LOW,
//     alternating.  The blue status LED (GPIO21) is ON during SD=HIGH and OFF
//     during SD=LOW, so the user always knows which polarity is active.
//   - A diagnostic line is printed once per second over USB-CDC.
//
// Interpretation:
//   * SD=HIGH plays AND SD=LOW is silent  -> SD is active-HIGH (as pins.h).
//   * SD=LOW  plays AND SD=HIGH is silent -> SD is active-LOW, flip
//     AMP_SD_ACTIVE_LEVEL in pins.h.
//   * Loud continuous tone on BOTH -> chain OK, both polarities partly work.
//   * No sound on EITHER polarity  -> amp/speaker/power/input hard fault.
//
// Library note: arduino-esp32 3.20017 core declares the USB-CDC stream as
// `Serial` only under ARDUINO_USB_MODE=1, so we keep that build flag.

#include <Arduino.h>
#include "USB.h"
#include "pins.h"

static const int  kCh    = AUDIO_PWM_CHANNEL;
static const int  kRes   = AUDIO_PWM_RES_BITS;   // 10 bits 0..1023
static const int  kFreq  = 440;                    // test tone Hz

void setup() {
    pinMode(LED_GPIO, OUTPUT);
    pinMode(AMP_SD_GPIO, OUTPUT);

    // Loud full-scale square wave via a plain LEDC duty toggle.
    ledcSetup(kCh, AUDIO_PWM_FREQ_HZ, kRes);
    ledcAttachPin(AUDIO_PWM_GPIO, kCh);
    ledcWrite(kCh, AUDIO_PWM_MAX);

    Serial.begin(115200);
    delay(500);              // let USB-CDC enumerate
    Serial.println("[ST] AUDIO_STRENGTH_TEST v3");
}

void loop() {
    const uint32_t now = millis();

    // 5 s SD=HIGH window, then 5 s SD=LOW, repeating.
    const bool sdHigh = ((now / 5000UL) % 2) == 0;

    // Blue status LED mirrors the polarity window (ON = SD HIGH now).
    digitalWrite(AMP_SD_GPIO, sdHigh ? HIGH : LOW);
    digitalWrite(LED_GPIO,    sdHigh ? HIGH : LOW);

    // 440 Hz full-scale square wave: toggle duty every half period.
    static uint32_t s_last = 0;
    static int      s_lev  = 0;
    const uint32_t nowUs = micros();
    if ((int32_t)(nowUs - s_last) >= (1000000UL / (2 * kFreq))) {
        s_last = nowUs;
        s_lev = !s_lev;
        ledcWrite(kCh, s_lev ? AUDIO_PWM_MAX : 0);
    }

    // 1 s diagnostic line so the host can verify GPIO20 is actually toggling.
    static uint32_t s_lastReport = 0;
    if (now - s_lastReport >= 1000) {
        s_lastReport = now;
        Serial.print("[ST] SD=");
        Serial.print(sdHigh ? "HIGH(5s)" : "LOW(5s)");
        Serial.print(" tone=440Hz full-duty GPIO20, t=");
        Serial.println(now);
    }
}