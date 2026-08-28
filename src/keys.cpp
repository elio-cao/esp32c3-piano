// keys.cpp
// 8-key touch scanner (digitalRead implementation).
//
// We rely on digitalRead() for every key.  The active level is set by
// KEY_ACTIVE_LEVEL in pins.h.  Each key gets a 30 ms debounce window.
// A single `keys_tick()` call samples raw state, applies debounce, and
// updates the "last pressed" index used by the audio engine.

#include "keys.h"
#include "notes.h"

#include <Arduino.h>
#include <string.h>

// USB-CDC `Serial` is not consistently exposed across Arduino-ESP32
// 3.x point releases, so we deliberately avoid it here.  All logging
// goes through the macro below; if you want to re-enable logs over the
// USB-CDC port, just `#include "USB.h"` and replace DBG(...) with
// `Serial.print(...)`.
#define DBG(...)  do { } while (0)

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
static bool s_raw[NUM_KEYS];
static bool s_state[NUM_KEYS];
static uint32_t s_rawChangedAt[NUM_KEYS];
static int s_lastPressed = -1;
static bool s_initialised = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void sampleRaw() {
    for (int i = 0; i < NUM_KEYS; ++i) {
        if (kKeyGpio[i] < 0) { s_raw[i] = false; continue; } // disabled slot
        const int level = digitalRead(kKeyGpio[i]);
        s_raw[i] = (level == KEY_ACTIVE_LEVEL);
    }
}

static void debounceAndUpdate() {
    const uint32_t now = millis();
    for (int i = 0; i < NUM_KEYS; ++i) {
        if (s_raw[i] != s_state[i]) {
            // Edge detected; start (or refresh) the debounce window.
            s_rawChangedAt[i] = now;
        } else if ((now - s_rawChangedAt[i]) >= KEY_DEBOUNCE_MS) {
            // Stable for the full window; promote to confirmed state.
            s_state[i] = s_raw[i];
        }
    }

    // Pick the highest-index pressed key as the note to play.  This
    // gives the synth a "last note wins" feel and keeps it monophonic.
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
    for (int i = 0; i < NUM_KEYS; ++i) {
        s_rawChangedAt[i] = 0;
        if (kKeyGpio[i] < 0) continue;                // disabled slot
        // Enable internal pull-up so an unconnected pad reads HIGH
        // (inactive) by default.  This matches KEY_ACTIVE_LEVEL = HIGH.
        pinMode(kKeyGpio[i], INPUT_PULLUP);
    }
    s_lastPressed = -1;

    s_initialised = true;
    DBG("[keys] initialised (digitalRead on 8 GPIOs)");
}

bool keys_tick(void) {
    if (!s_initialised) return false;
    sampleRaw();
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
    DBG("[keys] GPIO map: ");
    for (int i = 0; i < NUM_KEYS; ++i) {
        DBG("K%d=GPIO%d ", i, (int)kKeyGpio[i]);
    }
    DBG("\n[keys] Last pressed: ");
    if (s_lastPressed < 0) {
        DBG("(none)");
    } else {
        DBG("key %d (%s)", s_lastPressed, kKeyNoteNames[s_lastPressed]);
    }
}
