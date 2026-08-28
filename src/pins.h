// pins.h
// All GPIO assignments for the ESP32-C3 electronic piano.
// Pin numbers are taken from the 16-pin SMD module (ESP32-C3FH4 / ESP32-C3FN4
// in DevKitM form factor). The mapping is fully cross-checked against the
// netlist $NETS section in `ESP32C3电子琴2026-08-28.tel`, so DO NOT edit
// these unless you are deliberately re-wiring the hardware.
//
// Pin index (logical) 0..7 maps directly to the user-facing key 1..8.
//  - key 0..5  -> TP1..TP6, ESP32-C3 internal touch HAL on GPIO2..7
//  - key 6..7  -> TP7, TP8 read through TTP223 hardware on GPIO1, GPIO0
//
// See README.md and the planning artifact for the full pin table.
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
// U3 is held in shutdown by R5 (10 k pull-down). Driving GPIO10 high wakes
// the part. Keep it LOW at boot so we do not pop the speaker.
#define AMP_SD_GPIO           GPIO_NUM_10
#define AMP_SD_ACTIVE_LEVEL   HIGH

// ----- Status LED ---------------------------------------------------------
// GPIO21 -> R1 (1k) -> LED1 -> GND. Blinks on boot, steady on while a key
// is being pressed.
#define LED_GPIO              GPIO_NUM_21
#define LED_ACTIVE_LEVEL      HIGH
#define LED_OFF_LEVEL         LOW

// ----- Touch keys ---------------------------------------------------------
// Six keys go through the ESP32-C3 internal touch peripheral (touch_pad_*).
// Each key is paired with one TOUCH_PAD_NUMx constant that the HAL expects.
//
// key 0  -> TP1  -> GPIO2 -> TOUCH_PAD_NUM2
// key 1  -> TP2  -> GPIO3 -> TOUCH_PAD_NUM3
// key 2  -> TP3  -> GPIO4 -> TOUCH_PAD_NUM4
// key 3  -> TP4  -> GPIO5 -> TOUCH_PAD_NUM5
// key 4  -> TP5  -> GPIO6 -> TOUCH_PAD_NUM6
// key 5  -> TP6  -> GPIO7 -> TOUCH_PAD_NUM7
// key 6  -> TP7  -> GPIO1  (TTP223 digital read, NOT the touch HAL)
// key 7  -> TP8  -> GPIO0  (TTP223 digital read, NOT the touch HAL)
#define NUM_KEYS              8
#define NUM_TOUCH_HAL_KEYS    6
#define NUM_TTP223_KEYS       2

// Indices into g_touchPads[]. Order matches key 0..5.
static const int kTouchHalGpios[NUM_TOUCH_HAL_KEYS] = {
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
};

#include "driver/touch_pad.h"
static const touch_pad_t kTouchHalPads[NUM_TOUCH_HAL_KEYS] = {
    TOUCH_PAD_NUM2,
    TOUCH_PAD_NUM3,
    TOUCH_PAD_NUM4,
    TOUCH_PAD_NUM5,
    TOUCH_PAD_NUM6,
    TOUCH_PAD_NUM7,
};

// TTP223 hardware-touch key wiring. Index into the array matches the
// logical key index (6 and 7).
static const int kTtp223Gpios[NUM_TTP223_KEYS] = {
    GPIO_NUM_1,  // TP7  -> TTP223(U4)  -> R3 -> GPIO1
    GPIO_NUM_0,  // TP8  -> TTP223(U16) -> R4 -> GPIO0
};
#define TTP223_ACTIVE_LEVEL   HIGH

// ----- Scan timing --------------------------------------------------------
#define KEY_SCAN_PERIOD_MS    20
#define KEY_DEBOUNCE_MS       30
#define TTP223_WARMUP_MS      1000    // TP223 0.5 s stable time + margin

// ----- Touch HAL tuning --------------------------------------------------
// ESP32-C3 raw readings GROW when the pad is touched. We capture a baseline
// at boot and trigger on raw > baseline + this offset.
#define TOUCH_BASELINE_SAMPLES  16
#define TOUCH_THRESHOLD_OFFSET  30
#define TOUCH_HVOLT             TOUCH_HVOLT_2V7
#define TOUCH_LVOLT             TOUCH_LVOLT_0V5
#define TOUCH_HVOLT_ATTEN       TOUCH_HVOLT_ATTEN_1V5

// ----- Audio envelope ----------------------------------------------------
// Each new note fades in over 12 ms to suppress key-click pops, and the
// note fades out over 50 ms once the key is released.
#define AUDIO_FADE_IN_MS        12
#define AUDIO_FADE_OUT_MS       50

#endif  // PINS_H
