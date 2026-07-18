#include "boardConfig.h"

// Important: Actual pin assignment for HW_VERSION_4_6 Board which is the one I use currently 
// const HardwarePins currentPins = {
//     .RANDOM_SEED_pin = 36,
//     .pin_swf00 = 39,
//     .pin_swf01 = 34,
//     .pin_swf02 = 35,
//     .pin_swf03 = 32,
//     .pin_dip00 = 5,
//     .pin_dip01 = 26,
//     .pin_dip02 = 25,
//     .pin_dip03 = 33,
//     .pin_ENC_A = 27,
//     .pin_ENC_B = 14,
//     .BUZZER_pin = 19,
//     .pin_rgb_led = 13,
//     .relayPin01 = 15,
//     .relayPin02 = 2,
//     .relayPin03 = 4,
//     .relayPin04 = 16,
//     .LED_IS = 18,
//     .LED_WIFI = 12,
//     .HWD_HB_PIN = 17,
//     .pin_ENC_D_SW = 23
// };


const HardwarePins boardPins = {
    .relayPin = {15, 2, 4, 16},
    .dipPin = {5, 26, 25, 33, 39, 34, 35, 32},
    .randomSeedPin = 36,
};

uint8_t dipValue = 0;
uint8_t addressValue = 0;
uint8_t functionValue = 0;

void boardConfig_readDipSwitches() {
    dipValue = 0;
    for (uint8_t i = 0; i < 8; i++) {
        pinMode(boardPins.dipPin[i], INPUT);
    }
    // Switches pull the pin LOW when ON.
    for (uint8_t i = 0; i < 8; i++) {
        if (!digitalRead(boardPins.dipPin[i])) {
            dipValue |= (1 << i);
        }
    }
    // Split into the address switch (sw1, low nibble) and function switch
    // (sw2, high nibble).
    addressValue = dipValue & 0x0F;
    functionValue = (dipValue >> 4) & 0x0F;
}
