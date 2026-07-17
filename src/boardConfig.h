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

void boardConfig_readDipSwitches();
