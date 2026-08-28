// keys.h
// 8-key touch scanner.
//
// Implementation note: this firmware uses plain digitalRead() on every
// key.  The Arduino-ESP32 3.20017 SDK ships a broken `driver/touch_pad.h`
// for the ESP32-C3 (it #includes `driver/touch_sensor.h` which is S3-only),
// so the ESP-IDF touch HAL cannot be compiled in this environment.  The
// six "native touch" pins (GPIO2..GPIO7) are therefore configured as
// digital inputs with an internal pull-up; they will read HIGH when the
// pad is touched and idle LOW.  If your PCB uses a TTP223 (or a bare pad
// with a 100 kOhm pull-up), the same logic applies.  Adjust the
// KEY_ACTIVE_LEVEL constant in pins.h if your hardware is active-low.

#ifndef KEYS_H
#define KEYS_H

#include <Arduino.h>

#include "pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Bring up the GPIO inputs and the debounce state machine.  Must be
/// called from setup() AFTER a delay of at least TTP223_WARMUP_MS so
/// that the TTP223 power-on stable period has elapsed.
void keys_init(void);

/// Re-sample every key and update the debounced state.  Call from
/// loop() at least every KEY_SCAN_PERIOD_MS.  Returns true if any key
/// is currently considered pressed.
bool keys_tick(void);

/// Copy the debounced state of all 8 keys into `out`.  `out` must
/// point to at least NUM_KEYS bools.  Index 0 = leftmost physical
/// key (C4), index 7 = rightmost (C5).
void keys_getState(bool *out);

/// Index of the highest-numbered key currently pressed, or -1 if no
/// key is pressed.  The audio engine uses this to pick the note.
int keys_lastPressed(void);

/// Pretty-print a one-line status summary over USB-CDC serial.
void keys_logStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // KEYS_H
