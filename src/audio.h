// audio.h
// Single-voice monophonic synthesizer that drives the LEDC PWM channel
// connected to GPIO20. The on-board R2/C1 RC network (~1.6 kHz corner)
// smooths the PWM into a low-fi audio signal that the external amplifier
// can drive the speaker with.
//
// The module is intentionally lock-free and ISR-friendly:
//   - audio_setFrequency() is called from the main loop when a key changes
//   - the hardware timer ISR reads the cached frequency, advances a phase
//     accumulator and writes a new duty cycle every sample
//   - when no key is pressed, audio_setFrequency(0) silences the channel
//     and the ISR simply holds the silent duty cycle
#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>

// Bring in the GPIO/LEDC constants from pins.h.
#include "pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Bring up the LEDC peripheral, build the sine lookup table, install the
/// sample-rate timer ISR and park the output on a silent mid-rail duty.
void audio_init(void);

/// Request a new note. Pass 0 to silence the channel; the ISR applies a
/// short fade-out so the speaker does not pop. Safe to call from the main
/// loop only (not from ISR).
void audio_setFrequency(float freqHz);

/// Returns the currently requested frequency in Hz. 0 means silent.
float audio_getFrequency(void);

/// True if the ISR is currently outputting non-silent samples.
bool audio_isActive(void);

/// Drive the envelope state machine.  Call from loop() at ~200 Hz
/// (every 5 ms).  Defined as a weak symbol in audio.cpp so the user
/// can override it (e.g. for a different envelope curve).
void audio_tick(void);

#ifdef __cplusplus
}
#endif

#endif  // AUDIO_H
