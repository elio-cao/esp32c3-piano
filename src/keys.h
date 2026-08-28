// keys.h
// 8-key touch scanner with two underlying drivers:
//   * keys 0..5  -> ESP32-C3 internal touch peripheral (touch_pad_* HAL)
//   * keys 6..7  -> TTP223 digital outputs on GPIO1 / GPIO0
//
// The module exposes a single "any key pressed" boolean and a per-key
// debounced state vector.  Call keys_init() once from setup(), then
// keys_tick() periodically (e.g. every 20 ms) from loop().  The audio
// engine and LED are driven by the caller in response to the returned
// state.
#ifndef KEYS_H
#define KEYS_H

#include <Arduino.h>

#include "pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Bring up the touch peripheral and configure the two TTP223 inputs.
/// Must be called from setup() AFTER audio_init() and AFTER a delay of at
/// least TTP223_WARMUP_MS, so that the TTP223 0.5 s power-on stable period
/// has elapsed.  Internally captures the touch baseline for the 6 HAL
/// channels.
void keys_init(void);

/// Re-sample the touch hardware and update the debounced key state.  Call
/// from loop() at least every KEY_SCAN_PERIOD_MS.  Returns the OR of all
/// key states (true if any key is currently considered pressed).
bool keys_tick(void);

/// Copy the current debounced state of all NUM_KEYS keys into `out`.
/// `out` must point to at least NUM_KEYS bools.  key index 0 is the
/// leftmost physical key (C4) and index 7 is the rightmost (C5).
void keys_getState(bool *out);

/// Index of the highest-numbered key currently pressed, or -1 if no key is
/// pressed.  Used by the audio engine to pick the note to play.
int keys_lastPressed(void);

/// Pretty-print a one-line summary over USB-CDC serial for diagnostics.
void keys_logStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // KEYS_H
