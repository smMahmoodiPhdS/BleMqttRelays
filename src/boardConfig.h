#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// BOARD VARIANT — selected at build time in platformio.ini
//
//   BOARD_PROTO_4_6      (default) the HW_4_6 protoboard / 4-slider test rig.
//                        Relays on 15/2/4/16. No coil feedback wired.
//   BOARD_ACTUATOR_BLE   the actuator-node-Ble PCB. Relays on 13/14/25/26,
//                        coil feedback on 34/35/36/39.
//
// Two physically different boards share this firmware, and the maps overlap in
// hostile ways: on the PCB, GPIO15 and GPIO4 are DIP inputs with 10k pull-ups
// and GPIO2 drives an LED, so flashing the protoboard map onto the PCB would
// drive outputs straight into those. Keep the variant flag honest, and keep
// OTA_HARDWARE_VERSION distinct per variant so an OTA push cannot cross over.
// ---------------------------------------------------------------------------
#if !defined(BOARD_PROTO_4_6) && !defined(BOARD_ACTUATOR_BLE)
#define BOARD_PROTO_4_6
#endif
#if defined(BOARD_PROTO_4_6) && defined(BOARD_ACTUATOR_BLE)
#error "Define exactly one of BOARD_PROTO_4_6 / BOARD_ACTUATOR_BLE"
#endif

// ---------------------------------------------------------------------------
// Pin map reference for the actuator-node-Ble PCB
// (Hardware/kicad/actuator-node-Ble, source of truth: ble_relay.py -> ble_relay.net)
//
// Relay channel (x4):
//   RLn_IO --> SS-13D07 1P3T slider throw C ("AUTO")
//              throw A = +3V3  ("ALWAYS ON"), throw B = GND ("OFF")
//         --> COM --> 1k --> AO3400 gate (100k gate pulldown) --> coil low side
//   So the ESP only owns the relay when the slider is in AUTO. Driving RLn_IO
//   HIGH energizes the coil.
//
// Relay feedback (x4, NEW on this board):
//   RLn_SW (FET drain / coil low side) --> 10k --+--> SENn
//                                                 |
//                                                20k
//                                                 |
//                                                GND
//   Coil energized  -> drain ~0 V   -> SENn ~0 mV      (LOW)
//   Coil released   -> drain ~5 V   -> SENn ~3333 mV   (HIGH)
//   => SENn is INVERTED: LOW means the relay is ON.
//   This reads the true coil state no matter who set it - MQTT, BLE, or the
//   on-board slider - which is the whole point of the feedback channel.
//
//   SEN1..SEN4 land on IO34 / IO35 / SENSOR_VP(IO36) / SENSOR_VN(IO39).
//   All four are INPUT-ONLY and all four are ADC1 channels (ADC1 keeps working
//   while WiFi is up; ADC2 would not). They have no internal pull resistors,
//   which is fine because the divider always defines the level.
// ---------------------------------------------------------------------------

struct HardwarePins {
    uint8_t relayPin[4];       // RL1_IO..RL4_IO   - coil drive (output, active HIGH)
    uint8_t relayFeedback[4];  // SEN1..SEN4       - coil sense (input only, INVERTED)
    uint8_t dipPin[8];         // DIP1..DIP8       - 10k pull-up to 3V3, switch to GND
    uint8_t ledStatus;         // LED_STATUS       - active HIGH, 0 = absent
    uint8_t ledBle;            // LED_BLE          - active HIGH, 0 = absent
    uint8_t buzzer;            // BUZZER           - BC817 base, active HIGH
    uint8_t rgbLed;            // RGB_LED          - WS2812B data
    uint8_t watchdogHeartbeat; // HWD_HB           - TLC555 kick, 0 = no external WD
    uint8_t i2cSda;            // SDA              - SH1106 OLED header
    uint8_t i2cScl;            // SCL

    // False when the coil-sense dividers are not wired. Reading unwired inputs
    // returns noise that would be reported as spurious relay overrides. When
    // false the firmware reports commanded state as measured state and never
    // claims an override - the pre-feedback behaviour, honestly labelled.
    bool relayFeedbackPresent;

    // True  -> read the divider with analogReadMilliVolts() and hysteresis.
    //          Requires ADC1 pins (32-39): ADC2 is unavailable while WiFi is up,
    //          which is a hard silicon limitation, not a driver quirk.
    // False -> plain digitalRead().
    //
    // Digital is perfectly adequate here. The divider presents ~3.33 V when the
    // coil is released and ~0.05 V when energized, against thresholds of
    // VIH 2.48 V and VIL 0.83 V - roughly 850 mV and 780 mV of margin. The only
    // thing lost is the millivolt reading used for diagnostics and for spotting
    // a half-driven gate, so RELAY_FB_UNKNOWN never fires in digital mode.
    bool relayFeedbackAnalog;
};

extern const HardwarePins boardPins;

// Analog thresholds for the SENn divider, in millivolts, with hysteresis.
// Nominal levels are 0 mV (energized) and ~3333 mV (released); anything in
// between is treated as "no decision yet" and the previous reading is kept.
#define RELAY_FB_MV_ON_MAX   1000   // below this -> coil energized
#define RELAY_FB_MV_OFF_MIN  1900   // above this -> coil released

// 0-255, one bit per DIP switch, read once at boot. Feeds the BLE UUID
// generation so multiple boards under the same farm owner don't collide.
extern uint8_t dipValue;

// The 8 DIP bits split into two 4-bit switches (see
// Docs/Architecture/Addressing-and-Topic-Design.md §1):
//   addressValue  = dipValue & 0x0F        (sw1, DIP1-4) -> module address 0..15
//   functionValue = (dipValue >> 4) & 0x0F (sw2, DIP5-8) -> function code 0..15
// NOTE: the docstring at the top of ble_relay.py states the opposite split
// ("DIP1-4 = FUNCTION, DIP5-8 = TYPE/ADDRESS"). The architecture doc and this
// firmware are the authority; the schematic comment needs correcting.
extern uint8_t addressValue;
extern uint8_t functionValue;

void boardConfig_readDipSwitches();
