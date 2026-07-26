#include "boardConfig.h"

// ---------------------------------------------------------------------------
// Two board variants, selected by the flag in boardConfig.h (set from
// platformio.ini). Keep both maps here rather than in git history: the PCB is
// the production target, the protoboard is the bench rig, and both are in
// active use.
// ---------------------------------------------------------------------------

#if defined(BOARD_ACTUATOR_BLE)

// --- actuator-node-Ble PCB -------------------------------------------------
// Every number below comes from the GPIO map block in
// Hardware/kicad/actuator-node-Ble/ble_relay.py, cross-checked against
// ble_relay.net. Do not edit one without the other.
const HardwarePins boardPins = {
    .relayPin          = {13, 14, 25, 26},               // RL1_IO..RL4_IO
    .relayFeedback     = {34, 35, 36, 39},               // SEN1..SEN4 (VP=36, VN=39)
    .dipPin            = {4, 5, 15, 17, 18, 19, 23, 27}, // DIP1..DIP8
    .ledStatus         = 2,
    .ledBle            = 12,
    .buzzer            = 32,
    .rgbLed            = 33,
    .watchdogHeartbeat = 16,                             // HWD_HB -> TLC555
    .i2cSda            = 21,
    .i2cScl            = 22,
    .relayFeedbackPresent = true,
    .relayFeedbackAnalog  = true,    // SEN1..SEN4 are all ADC1
};

#else   // BOARD_PROTO_4_6

// --- HW_4_6 protoboard / 4-slider relay test rig ---------------------------
//
// COIL FEEDBACK WIRING FOR THIS RIG — wire each relay's drain-sense divider
// (10k from the coil low side, 20k to GND, tap to the GPIO) as:
//
//     relay 1 -> GPIO14      relay 3 -> GPIO23
//     relay 2 -> GPIO18      relay 4 -> GPIO27
//
// Why these four, out of the free pins (0, 12, 14, 18, 23, 27, 36):
//
//   GPIO0   EXCLUDED — BOOT strap. A relay restored ON at power-up pulls this
//           LOW through the divider and the chip enters serial download mode
//           instead of booting.
//   GPIO12  EXCLUDED — MTDI strap, and this one is genuinely dangerous. GPIO12
//           must be LOW at reset or the chip selects a 1.8 V flash rail and
//           fails to boot. At power-on every relay is released, so the divider
//           presents ~3.33 V — GPIO12 would be HIGH at reset, every time.
//   GPIO36  reserved. It is the only ADC1 pin left free on this map; keeping it
//           unused leaves one analog input available for a future sensor.
//   14/18/23/27  chosen. No strapping role, no conflict with the OLED I2C pins
//           (21/22), no conflict with the buzzer (19), RGB LED (13) or watchdog
//           heartbeat (17).
//
// These are read DIGITALLY, not with the ADC. Only GPIO36 remains on ADC1, and
// ADC2 (which covers 12/14/27) cannot be read at all while WiFi is active —
// that is a documented silicon limitation of the ESP32, not something a driver
// update fixes. Digital has ample margin here: ~3.33 V released against a
// 2.48 V VIH, ~0.05 V energized against a 0.83 V VIL.
//
// Set .relayFeedbackPresent = false if the dividers are not fitted yet; the
// firmware then reports commanded state and never raises a false override.
const HardwarePins boardPins = {
    .relayPin          = {15, 2, 4, 16},
    .relayFeedback     = {14, 18, 23, 27},                // digital sense
    .dipPin            = {5, 26, 25, 33, 39, 34, 35, 32},
    .ledStatus         = 0,                               // 0 = not present
    .ledBle            = 0,
    .buzzer            = 19,
    .rgbLed            = 13,
    .watchdogHeartbeat = 17,                              // HWD_HB_PIN on this rig
    .i2cSda            = 21,
    .i2cScl            = 22,
    .relayFeedbackPresent = true,
    .relayFeedbackAnalog  = false,   // no ADC1 pins free — see the note above
};

#endif

uint8_t dipValue = 0;
uint8_t addressValue = 0;
uint8_t functionValue = 0;

void boardConfig_readDipSwitches() {
    dipValue = 0;
    // The DIP lines have external 10k pull-ups to 3V3 and the switch shorts to
    // GND, so plain INPUT is correct - no internal pull needed. That matters on
    // the protoboard map, where three DIP lines sit on input-only pins
    // (34/35/39) that could not take an internal pull anyway.
    for (uint8_t i = 0; i < 8; i++) {
        pinMode(boardPins.dipPin[i], INPUT);
    }
    delayMicroseconds(50);   // let the pull-ups settle after the mode change
    for (uint8_t i = 0; i < 8; i++) {
        if (!digitalRead(boardPins.dipPin[i])) {   // switch ON pulls the pin LOW
            dipValue |= (1 << i);
        }
    }
    // Split into the address switch (sw1, low nibble) and function switch
    // (sw2, high nibble).
    addressValue = dipValue & 0x0F;
    functionValue = (dipValue >> 4) & 0x0F;
}
