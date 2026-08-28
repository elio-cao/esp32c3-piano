// main.cpp
// ESP32-C3 8-key touch piano — production firmware.
//
// Loop: scan the 8 touch keys, map each to a note frequency, drive the
// monophonic synth (audio.cpp) and mirror key activity on the blue status
// LED (GPIO21). The amp is left enabled (AMP_SD_ACTIVE_LEVEL, which is LOW
// for the NS4165B) so pressing a key is heard immediately.
//
// Uses the plain-digital touch HAL workaround described in pins.h.

#include <Arduino.h>
#include "USB.h"

#include "pins.h"
#include "notes.h"
#include "audio.h"
#include "keys.h"

static bool  s_keyActive = false;
static float s_noteFreq  = 0.0f;

void setup() {
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF_LEVEL);

    pinMode(AMP_SD_GPIO, OUTPUT);
    // NS4165B SD is active-LOW; AMP_SD_ACTIVE_LEVEL==LOW enables the amp.
    digitalWrite(AMP_SD_GPIO, AMP_SD_ACTIVE_LEVEL);

    // Allow the USB-CDC / TTP223 to stabilise before reading the keys.
    digitalWrite(LED_GPIO, LED_ACTIVE_LEVEL);
    delay(TTP223_WARMUP_MS);
    digitalWrite(LED_GPIO, LED_OFF_LEVEL);

    keys_init();
    audio_init();

    // Boot indicator: three short blinks.
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_GPIO, LED_ACTIVE_LEVEL);
        delay(80);
        digitalWrite(LED_GPIO, LED_OFF_LEVEL);
        delay(80);
    }

    Serial.begin(115200);
    delay(300);
    Serial.println("[PIANO] boot OK, amp enabled (SD="
                   + String((int)AMP_SD_ACTIVE_LEVEL) + ")");
}

void loop() {
    const bool any = keys_tick();
    const int  k   = keys_lastPressed();

    float freq = 0.0f;
    if (k >= 0 && k < NUM_KEYS) {
        freq = kNoteFreq[k];
    }

    // Only touch the synth when the requested note actually changed.
    if (freq != s_noteFreq || any != s_keyActive) {
        audio_setFrequency(freq);
        s_noteFreq  = freq;
        s_keyActive = any;
    }

    digitalWrite(LED_GPIO, any ? LED_ACTIVE_LEVEL : LED_OFF_LEVEL);

    audio_tick();        // ~200 Hz envelope driver
    delay(5);
}