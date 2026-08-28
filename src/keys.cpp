// keys.cpp
// Touch scanner implementation.
//
// Two layers
// ----------
//  1. Per-key "raw" state, updated every call to keys_tick().
//       * HAL keys  : touch_pad_read_raw() compared to a per-channel
//         baseline captured at boot.
//       * TTP223    : digitalRead(), with active-high polarity per the
//         netlist (AHLB=GND, TOG=GND -> direct mode, active high).
//  2. Debounced stable state, kept in s_state[].  A key has to read raw
//     pressed for KEY_DEBOUNCE_MS in a row before it shows up as pressed.

#include "keys.h"
#include "notes.h"

#include <driver/touch_sensor.h>
#include <esp_log.h>

#include <Arduino.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
// Raw state from the most recent sample, true = pressed.
static bool s_raw[NUM_KEYS];
// Debounced state, true = confirmed pressed.
static bool s_state[NUM_KEYS];
// Timestamp (ms) when raw state last changed for each key.
static uint32_t s_rawChangedAt[NUM_KEYS];
// Baseline raw value captured at boot for each HAL channel.
static uint32_t s_baseline[NUM_TOUCH_HAL_KEYS];
// Last key reported as "currently pressed" for the audio engine.  -1 if
// nothing is pressed.
static int s_lastPressed = -1;
// True after keys_init() has finished.  Used to avoid touching the HAL
// before it is configured.
static bool s_initialised = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void setupTtp223Gpio() {
    for (int i = 0; i < NUM_TTP223_KEYS; ++i) {
        pinMode(kTtp223Gpios[i], INPUT_PULLUP);
    }
}

static void captureTouchBaselines() {
    ESP_LOGI("keys", "Capturing touch baseline (%d samples per channel)...",
             TOUCH_BASELINE_SAMPLES);

    for (int ch = 0; ch < NUM_TOUCH_HAL_KEYS; ++ch) {
        // Warm up the channel - first read after enable is often noisy.
        for (int i = 0; i < 4; ++i) {
            (void)touch_pad_read_raw(kTouchHalPads[ch]);
        }
        // Median of N samples is a reasonable noise-resistant baseline.
        uint32_t samples[TOUCH_BASELINE_SAMPLES];
        for (int i = 0; i < TOUCH_BASELINE_SAMPLES; ++i) {
            samples[i] = touch_pad_read_raw(kTouchHalPads[ch]);
        }
        // Simple insertion sort to extract the median.
        for (int i = 1; i < TOUCH_BASELINE_SAMPLES; ++i) {
            uint32_t v = samples[i];
            int j = i - 1;
            while (j >= 0 && samples[j] > v) {
                samples[j + 1] = samples[j];
                j--;
            }
            samples[j + 1] = v;
        }
        s_baseline[ch] =
            samples[TOUCH_BASELINE_SAMPLES / 2];
        ESP_LOGI("keys", "  HAL ch %d (GPIO%d) baseline = %lu",
                 ch, kTouchHalGpios[ch], (unsigned long)s_baseline[ch]);
    }
}

static void setupTouchHal() {
    esp_err_t err = touch_pad_init();
    if (err != ESP_OK) {
        ESP_LOGE("keys", "touch_pad_init failed: %s", esp_err_to_name(err));
        return;
    }
    err = touch_pad_set_voltage(TOUCH_HVOLT, TOUCH_LVOLT, TOUCH_HVOLT_ATTEN);
    if (err != ESP_OK) {
        ESP_LOGE("keys", "touch_pad_set_voltage failed: %s", esp_err_to_name(err));
    }
    for (int ch = 0; ch < NUM_TOUCH_HAL_KEYS; ++ch) {
        // Second argument to touch_pad_config is the slope / "touch pad
        // driver" - 0 maps to the recommended default on ESP32-C3.
        err = touch_pad_config(kTouchHalPads[ch], 0);
        if (err != ESP_OK) {
            ESP_LOGE("keys", "touch_pad_config ch %d failed: %s",
                     ch, esp_err_to_name(err));
        }
    }
    // A short settling pause before reading baselines.
    delay(50);
    captureTouchBaselines();
}

static void sampleRawTouchHal() {
    for (int ch = 0; ch < NUM_TOUCH_HAL_KEYS; ++ch) {
        const uint32_t raw = touch_pad_read_raw(kTouchHalPads[ch]);
        // ESP32-C3 raw grows when touched (positive polarity).
        const uint32_t threshold = s_baseline[ch] + TOUCH_THRESHOLD_OFFSET;
        s_raw[ch] = (raw > threshold);
    }
}

static void sampleRawTtp223() {
    for (int i = 0; i < NUM_TTP223_KEYS; ++i) {
        const int gpioIdx = NUM_TOUCH_HAL_KEYS + i;  // 6, 7
        const bool level = (digitalRead(kTtp223Gpios[i]) == TTP223_ACTIVE_LEVEL);
        s_raw[gpioIdx] = level;
    }
}

static void debounceAndUpdate() {
    const uint32_t now = millis();
    for (int i = 0; i < NUM_KEYS; ++i) {
        if (s_raw[i] != s_state[i]) {
            // Edge - start (or refresh) the debounce window.
            s_rawChangedAt[i] = now;
        } else if ((now - s_rawChangedAt[i]) >= KEY_DEBOUNCE_MS) {
            // Stable for the full window; promote to confirmed state.
            s_state[i] = s_raw[i];
        }
    }

    // Pick the highest-index pressed key as the note to play.  This
    // gives the player a "last note wins" feel and keeps the synth
    // monophonic.
    int last = -1;
    for (int i = 0; i < NUM_KEYS; ++i) {
        if (s_state[i]) last = i;
    }
    s_lastPressed = last;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void keys_init(void) {
    memset(s_raw, 0, sizeof(s_raw));
    memset(s_state, 0, sizeof(s_state));
    for (int i = 0; i < NUM_KEYS; ++i) s_rawChangedAt[i] = 0;
    memset(s_baseline, 0, sizeof(s_baseline));
    s_lastPressed = -1;

    setupTtp223Gpio();
    setupTouchHal();

    s_initialised = true;
    ESP_LOGI("keys", "keys_init done");
}

bool keys_tick(void) {
    if (!s_initialised) return false;
    sampleRawTouchHal();
    sampleRawTtp223();
    debounceAndUpdate();
    return s_lastPressed >= 0;
}

void keys_getState(bool *out) {
    memcpy(out, s_state, sizeof(s_state));
}

int keys_lastPressed(void) {
    return s_lastPressed;
}

void keys_logStatus(void) {
    Serial.print("[keys] HAL baselines: ");
    for (int ch = 0; ch < NUM_TOUCH_HAL_KEYS; ++ch) {
        Serial.printf("T%d=%lu ", ch + 2, (unsigned long)s_baseline[ch]);
    }
    Serial.println();
    Serial.print("[keys] Last pressed: ");
    if (s_lastPressed < 0) {
        Serial.println("(none)");
    } else {
        Serial.printf("key %d (%s)\n", s_lastPressed,
                      kKeyNoteNames[s_lastPressed]);
    }
}
