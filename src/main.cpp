// main.cpp  -  SELF-TEST / DIAGNOSTIC build (temporary)
//
// THIS IS A TEMPORARY DIAGNOSTIC VERSION, NOT THE REAL PIANO FIRMWARE.
//
// Goal: answer three questions in one flash, without touching any key:
//   (1) Is the firmware actually running?   -> boot blinks on GPIO21
//   (2) On which LED pin is the status LED? -> GPIO21 vs GPIO8
//   (3) Is the amplifier / speaker chain OK and what is the SD polarity?
//
// Behaviour (repeats forever):
//   - On boot, GPIO21 blinks 5 times (120 ms on / 120 ms off) as a marker.
//   - After that it alternates two windows, each 2 seconds:
//       [SD=HIGH window] GPIO21 LED ON,  GPIO8 OFF,  plays 659Hz then 494Hz
//       [SD=LOW  window] GPIO21 LED OFF, GPIO8 ON,   plays 494Hz then 659Hz
//   - So the LED tells you the current SD level window, and whether you hear
//     a note during that window tells you the working logic of the amp.
//
// Interpretation:
//   * No LED blinks at all          => the firmware is NOT running
//     (flash issue, boot strapping, or chip held in reset).
//   * One of the two LEDs blinks    => firmware IS running; the blinking pin
//     is your status LED (GPIO21 or GPIO8).
//   * Sound only while GPIO21 lit   => SD is active-HIGH (matches pins.h, OK).
//   * Sound only while GPIO8  lit   => SD is active-LOW (flip AMP_SD_ACTIVE_LEVEL).
//   * Never any sound              => speaker/amp/PWM or RC-filter path faulty.

#include <Arduino.h>

#include "audio.h"
#include "pins.h"

// Candidate alt status LED used by some board revisions (see README table).
#define LED2_GPIO       GPIO_NUM_8
#define LED2_ACTIVE     HIGH

static const float kHighFreq = 659.25f;  // E5
static const float kLowFreq  = 493.88f;  // B4

static void setAmpSd(int level) {
    digitalWrite(AMP_SD_GPIO, level);
}

void setup() {
    Serial.begin(115200);               // USB CDC (ARDUINO_USB_CDC_ON_BOOT=1)
    delay(500);                          // let the USB CDC enumerate
    Serial.println("[ST] boot begin");

    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF_LEVEL);
    pinMode(LED2_GPIO, OUTPUT);
    digitalWrite(LED2_GPIO, !LED2_ACTIVE);
    pinMode(AMP_SD_GPIO, OUTPUT);
    setAmpSd(!AMP_SD_ACTIVE_LEVEL);

    // Boot marker: 5 quick blinks on GPIO21.
    for (int j = 0; j < 5; ++j) {
        digitalWrite(LED_GPIO, LED_ACTIVE_LEVEL);
        delay(120);
        digitalWrite(LED_GPIO, LED_OFF_LEVEL);
        delay(120);
    }
    Serial.println("[ST] blinks done");

    Serial.println("[ST] audio_init ..");
    audio_init();
    audio_setFrequency(kHighFreq);
    Serial.println("[ST] audio_init done, should be playing now");
}

void loop() {
    const uint32_t now  = millis();
    const bool     sdHigh = ((now / 2000UL) % 2) == 0;  // 2s windows
    const bool     useLow = ((now / 1000UL) & 1) != 0;  // toggle each 1s

    setAmpSd(sdHigh ? AMP_SD_ACTIVE_LEVEL : (!AMP_SD_ACTIVE_LEVEL));

    // LED maps the SD window so the user knows the current phase.
    digitalWrite(LED_GPIO,  sdHigh ? LED_ACTIVE_LEVEL : LED_OFF_LEVEL);
    digitalWrite(LED2_GPIO, sdHigh ? (!LED2_ACTIVE)    : LED2_ACTIVE);

    audio_setFrequency(useLow ? kLowFreq : kHighFreq);

    // Keep the synthesis envelope advancing.
    for (int i = 0; i < 2; ++i) audio_tick();
    delay(1);

    // --- 1-second diagnostic report ---------------------------------------
    static uint32_t s_lastLog = 0;
    static uint32_t s_lastIsr = 0;
    if (millis() - s_lastLog >= 1000) {
        s_lastLog = millis();
        const uint32_t t = audio_getIsrTicks();
        Serial.print("[ST] t=");
        Serial.print(millis());
        Serial.print(" isr=");
        Serial.print(t);
        Serial.print("(+");
        Serial.print(t - s_lastIsr);
        Serial.print(") env=");
        Serial.print(audio_getEnvelope());
        Serial.print(" f=");
        Serial.print(audio_getFrequency(), 1);
        Serial.print(" Hz SD=");
        Serial.println(digitalRead(AMP_SD_GPIO) ? "HIGH" : "LOW");
        s_lastIsr = t;
    }
}