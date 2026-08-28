// notes.h
// Note frequencies for the 8-key electronic piano. Default mapping is the
// C major scale over one octave (C4 .. C5) with the 8th key landing on C5.
//
// key 0 (TP1, GPIO2 / T2)  -> C4  261.63 Hz
// key 1 (TP2, GPIO3 / T3)  -> D4  293.66 Hz
// key 2 (TP3, GPIO4 / T4)  -> E4  329.63 Hz
// key 3 (TP4, GPIO5 / T5)  -> F4  349.23 Hz
// key 4 (TP5, GPIO6 / T6)  -> G4  392.00 Hz
// key 5 (TP6, GPIO7 / T7)  -> A4  440.00 Hz
// key 6 (TP7, GPIO1 / TTP223)  -> B4  493.88 Hz
// key 7 (TP8, GPIO0 / TTP223)  -> C5  523.25 Hz
//
// Edit this array freely; the audio engine accepts any frequency. The
// "kKeyNoteNames" array is for serial logging only.
#ifndef NOTES_H
#define NOTES_H

#include "pins.h"   // NUM_KEYS

static const float kNoteFreq[NUM_KEYS] = {
    261.63f,  // C4
    293.66f,  // D4
    329.63f,  // E4
    349.23f,  // F4
    392.00f,  // G4
    440.00f,  // A4
    493.88f,  // B4
    523.25f,  // C5
};

static const char *const kKeyNoteNames[NUM_KEYS] = {
    "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5",
};

#endif  // NOTES_H
