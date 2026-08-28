// main.cpp
// Top-level glue for the ESP32-C3 electronic piano.  Wires together the
// audio engine, the touch scanner and the amplifier-enable / status LED.
//
// USB-CDC logging is intentionally disabled because `Serial` is not
// consistently resolved by the Arduino-ESP32 3.x point releases shipped
// on the platform (the `driver/touch_pad.h` is also broken on the C3
// toolchain, which is why keys.cpp falls back to digitalRead).  If
// you need boot logs, replace DBG(...) below with `Serial.print(...)`
// and add `#include "USB.h"` to this file.

#include <Arduino.h>

#include "audio.h"
#include "keys.h"
#include "notes.h"
#include "pins.h"

#define DBG(...)  do { } while (0)

// Track the note that is currently feeding the synth.  When the same key
// index is held, we do not want to re-trigger the envelope every tick.
static int s_playingKey = -1;

// LED blink on boot.  We light the LED for 80 ms three times with 80 ms
// gaps in between.
static void ledSelfTest() {
    for (int i = 0; i < 3; ++i) {
        digitalWrite(LED_GPIO, LED_ACTIVE_LEVEL);
        delay(80);
        digitalWrite(LED_GPIO, LED_OFF_LEVEL);
        delay(80);
    }
}

void setup() {
    // Configure the GPIO-controlled outputs up front.
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF_LEVEL);
    pinMode(AMP_SD_GPIO, OUTPUT);
    digitalWrite(AMP_SD_GPIO, !AMP_SD_ACTIVE_LEVEL);  // muted at boot

    // Boot-time LED bling to confirm the LED is wired correctly.
    ledSelfTest();

    // TTP223 has a 0.5 s power-on stable window during which touch
    // detection is disabled.  Wait the full second so that the very
    // first scan after keys_init() sees a stable part.
    DBG("[main] Waiting 1.0 s for TTP223 warm-up...\n");
    delay(TTP223_WARMUP_MS);

    // Initialise the audio engine before the keys so that the first
    // press already has a configured PWM channel.
    audio_init();
    DBG("[main] audio_init done\n");

    keys_init();
    keys_logStatus();

    DBG("[main] Ready - touch a key to play a note\n");
}

void loop() {
    static uint32_t lastKeysTick = 0;
    static uint32_t lastEnvTick   = 0;

    const uint32_t now = millis();

    // Touch scan at KEY_SCAN_PERIOD_MS cadence.
    if ((now - lastKeysTick) >= KEY_SCAN_PERIOD_MS) {
        lastKeysTick = now;
        const bool anyPressed = keys_tick();
        const int pressed = keys_lastPressed();

        if (pressed != s_playingKey) {
            if (pressed >= 0) {
                audio_setFrequency(kNoteFreq[pressed]);
                digitalWrite(AMP_SD_GPIO, AMP_SD_ACTIVE_LEVEL);
                digitalWrite(LED_GPIO, LED_ACTIVE_LEVEL);
                DBG("[main] Note on  key %d %s (%.2f Hz)\n",
                    pressed, kKeyNoteNames[pressed], kNoteFreq[pressed]);
            } else {
                audio_setFrequency(0.0f);
                // We leave the LED on for a moment so the player sees
                // the key release; the amplifier mute is handled by the
                // audio envelope returning to zero.
                digitalWrite(LED_GPIO, LED_OFF_LEVEL);
                digitalWrite(AMP_SD_GPIO, !AMP_SD_ACTIVE_LEVEL);
                DBG("[main] Note off\n");
            }
            s_playingKey = pressed;
        }
        (void)anyPressed;  // kept for symmetry / future logging
    }

    // Envelope update at ~5 ms cadence (200 Hz).  The audio engine
    // exposes audio_tick() as a weak symbol; calling it advances the
    // fade-in / fade-out state.
    if ((now - lastEnvTick) >= 5) {
        lastEnvTick = now;
        audio_tick();
    }

    // We deliberately do not delay() here - the timers above handle
    // the cadence.  A short no-op yield keeps the watchdog happy on
    // some revisions of the ESP-IDF scheduler.
    delay(1);
}
