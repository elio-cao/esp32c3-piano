// pins.h
// All GPIO assignments for the ESP32-C3 electronic piano.
// Pin numbers are taken from the 16-pin SMD module (ESP32-C3FH4 / ESP32-C3FN4
// in DevKitM form factor). The mapping is fully cross-checked against the
// netlist $NETS section in `ESP32C3电子琴2026-08-28.tel`, so DO NOT edit
// these unless you are deliberately re-wiring the hardware.
//
// Pin index (logical) 0..7 maps directly to the user-facing key 1..8.
//  - key 0..5  -> TP1..TP6
//  - key 6..7  -> TP7, TP8
//
// IMPORTANT — touch HAL workaround
// ---------------------------------
// The Arduino-ESP32 3.20017 framework ships a broken `driver/touch_pad.h`
// for the ESP32-C3: it `#include "driver/touch_sensor.h"` which is a
// S3-only header and does not exist for the C3 toolchain.  As a result
// every compilation of code that pulls in the ESP-IDF touch HAL fails
// with `fatal error: driver/touch_sensor.h: No such file or directory`.
// We therefore rely on `digitalRead()` (with internal pull-up) for the
// six native-touch channels.  This requires the touch electrodes to be
// wired so that an idle key reads LOW and a touched key reads HIGH
// (which is the active-high output of a TTP223, and also the natural
// behaviour of a bare touch pad pulled up to VCC through a 100 kOhm
// resistor with the human body providing the pull-down to GND).
// See README.md for the full rationale and what to do if your PCB
// is wired differently.

#ifndef PINS_H
#define PINS_H

#include <Arduino.h>

// ----- Audio --------------------------------------------------------------
// LEDC PWM output for the amplifier. The ESP32-C3 DevKitM board does NOT
// expose GPIO18, so we cannot use the DAC. Instead we generate a 10-bit PWM
// at 88.2 kHz on GPIO20 and let the R2/C1 RC network smooth it into a
// quasi-analog waveform.
#define AUDIO_PWM_GPIO        GPIO_NUM_20
#define AUDIO_PWM_CHANNEL     0
#define AUDIO_PWM_RES_BITS    10      // 0..1023
#define AUDIO_PWM_FREQ_HZ     88200
#define AUDIO_PWM_DUTY_SILENT (1 << (AUDIO_PWM_RES_BITS - 1))  // 512 = mid-rail
#define AUDIO_PWM_MAX         ((1 << AUDIO_PWM_RES_BITS) - 1) // 1023

// ----- Amplifier enable ---------------------------------------------------
// U3 = NS4165B (eSOP8 AB/D audio amp). From the datasheet:
//   SD (pin1): LOW  = normal operation (enabled)
//              HIGH = shutdown (muted)
// GPIO10 -> R5 (10k pull-down) -> SD. Because the pull-down already keeps
// SD LOW, the part is enabled by default; the only reason to now drive the
// pin is to force shut-down, so the ACTIVE level here is the ENABLE level.
#define AMP_SD_GPIO           GPIO_NUM_10
#define AMP_SD_ACTIVE_LEVEL   LOW

// ----- Status LED ---------------------------------------------------------
// GPIO21 -> R1 (1k) -> LED1 -> GND. Blinks on boot, steady on while a key
// is being pressed.
#define LED_GPIO              GPIO_NUM_21
#define LED_ACTIVE_LEVEL      HIGH
#define LED_OFF_LEVEL         LOW

// ----- Touch keys ---------------------------------------------------------
// 8 keys total.  The mapping matches the netlist:
//
//   key 0  -> TP1 -> GPIO2
//   key 1  -> TP2 -> GPIO3
//   key 2  -> TP3 -> GPIO4
//   key 3  -> TP4 -> GPIO5
//   key 4  -> TP5 -> GPIO6
//   key 5  -> TP6 -> GPIO7
//   key 6  -> TP7 -> GPIO1   (TTP223 Q output, active high)
//   key 7  -> TP8 -> GPIO0   (TTP223 Q output, active high)
//
// All eight inputs use `digitalRead()` against a configurable active
// level.  Set the matching `_ACTIVE_LEVEL` to HIGH for TTP223, or to LOW
// for an open-collector / open-drain output.
#define NUM_KEYS              8

static const int kKeyGpio[NUM_KEYS] = {
    GPIO_NUM_2,  // key 0 (TP1)
    GPIO_NUM_3,  // key 1 (TP2)
    GPIO_NUM_4,  // key 2 (TP3)
    GPIO_NUM_5,  // key 3 (TP4)
    GPIO_NUM_6,  // key 4 (TP5)
    GPIO_NUM_7,  // key 5 (TP6)
    GPIO_NUM_1,  // key 6 (TP7) - via TTP223 U4
    GPIO_NUM_0,  // key 7 (TP8) - via TTP223 U16
};

#define KEY_ACTIVE_LEVEL      HIGH

// ----- Scan timing --------------------------------------------------------
#define KEY_SCAN_PERIOD_MS    20
#define KEY_DEBOUNCE_MS       30
#define TTP223_WARMUP_MS      1000    // TTP223 0.5 s stable time + margin

// ----- Audio envelope ----------------------------------------------------
#define AUDIO_FADE_IN_MS        12
#define AUDIO_FADE_OUT_MS       50

#endif  // PINS_H
