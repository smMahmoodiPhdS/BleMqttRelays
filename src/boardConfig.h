#pragma once
#include <Arduino.h>

struct HardwarePins {
    uint8_t relayPin[4];
    uint8_t dipPin[8];
    uint8_t randomSeedPin;
};

extern const HardwarePins boardPins;

// 0-255, one bit per DIP switch, read once at boot. Feeds the BLE UUID
// generation so multiple boards under the same farm owner don't collide.
extern uint8_t dipValue;

// The 8 DIP bits split into two 4-bit switches:
//   addressValue  = dipValue & 0x0F        (sw1) -> module address (0..15)
//   functionValue = (dipValue >> 4) & 0x0F (sw2) -> function code   (0..15)
// (Physical pin->bit mapping to be confirmed on the PCB.)
extern uint8_t addressValue;
extern uint8_t functionValue;

void boardConfig_readDipSwitches();
