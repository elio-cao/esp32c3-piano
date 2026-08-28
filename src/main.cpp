// main.cpp  —  AUDIO GPIO SCAN TEST (temporary, v5)
//
// Goal: find which ESP32-C3 GPIO actually drives the onboard amplifier input.
// We previously assumed AUDIO_PWM_GPIO = GPIO20, but only verified the LED pin
// (GPIO21) matched the 16-pin module map. If the module maps the amp pin to a
// different GPIO, GPIO20 is floating and the amp only passes its own switching
// noise to the speaker — exactly the "sand hiss" with no tone.
//
// Behaviour (loops forever):
//   - Amp SD driven to AMP_SD_ACTIVE_LEVEL (LOW for NS4165B) so it stays ON.
//   - Each candidate GPIO in turn is driven with a loud full-scale 880 Hz
//     square wave for ~1.5 s; all the others are left high-impedance.
//   - A line is printed over USB-CDC naming the current "#N GPIOxx".
//   - The manufacturer/amp chain only sounds on the GPIO that is actually wired
//     to the amp input.  To diagnose, just listen and report which #N beeps.

#include <Arduino.h>
#include "USB.h"
#include "pins.h"

// Candidates most likely to be the amp input pin on the 16-pin SMD module,
// in order of likelihood (GPIO20 per the original README first).
static const int kCand[] = {
    GPIO_NUM_20, GPIO_NUM_4,  GPIO_NUM_3,  GPIO_NUM_2,
    GPIO_NUM_5,  GPIO_NUM_6,  GPIO_NUM_7,  GPIO_NUM_0,
};
static const int kN = (int)(sizeof(kCand) / sizeof(kCand[0]));

void setup() {
    // --- Hardware workaround: netlist pulls GPIO0 to +5V via R4 -------------
    // ESP32-C3 absolute maximum on any IO is VIO+0.3V (≈3.6V with 3V3 IO),
    // so a 5V pull-up on GPIO0 forces the pad into clamp current and the
    // chip cannot reach the application.  Calling gpio_reset_pin() releases
    // any stray peripheral mux (USB-Serial/JTAG, MTCK, etc.), and leaving
    // GPIO0 in INPUT_DISABLE keeps it from sourcing/sinking any current.
    // This is the "software-only bypass" for key 7 (TP8).  The permanent fix
    // is to move R4 from +5V to 3V3 on the PCB.
    gpio_reset_pin((gpio_num_t)0);
    pinMode(0, INPUT_DISABLE);

    pinMode(LED_GPIO, OUTPUT);
    pinMode(AMP_SD_GPIO, OUTPUT);
    digitalWrite(AMP_SD_GPIO, AMP_SD_ACTIVE_LEVEL);   // LOW = NS4165B enabled

    for (int i = 0; i < kN; i++) {
        pinMode(kCand[i], INPUT);                      // all high-impedance
    }

    Serial.begin(115200);
    delay(300);
    Serial.println("[SCAN] audio GPIO sweep; SD=LOW(enabled)");
    Serial.print("candidates:");
    for (int i = 0; i < kN; i++) Serial.printf(" #%d=GPIO%d", i, kCand[i]);
    Serial.println();
}

void loop() {
    const int phase = (int)((millis() / 1500) % kN);
    const int cur   = kCand[phase];

    static int s_lastPhase = -1;
    if (phase != s_lastPhase) {
        if (s_lastPhase >= 0) pinMode(kCand[s_lastPhase], INPUT); // release old
        pinMode(cur, OUTPUT);
        digitalWrite(cur, LOW);
        s_lastPhase = phase;
        Serial.printf("#%d GPIO%d -> 880Hz tone\n", phase, cur);
    }

    // 880 Hz full-scale square wave via a plain digital toggle.
    static uint32_t s_lastUs = 0;
    static int      s_hi     = 0;
    const uint32_t  u = micros();
    if ((int32_t)(u - s_lastUs) >= 568) {   // ~880 Hz half period
        s_lastUs = u;
        s_hi = !s_hi;
        digitalWrite(cur, s_hi ? HIGH : LOW);
    }

    // Status LED heartbeat so the board is visibly alive.
    digitalWrite(LED_GPIO, ((millis() / 500) % 2) ? HIGH : LOW);
    delay(1);
}