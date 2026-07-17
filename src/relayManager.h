#pragma once
#include <Arduino.h>

#define RELAY_COUNT 4

// Called whenever a relay's state changes, regardless of whether the change
// came from BLE or MQTT, so the other transport can stay in sync.
typedef void (*RelayStateListener)(uint8_t relayIndex, bool state);

void setup_relays();
void relay_setState(uint8_t relayIndex, bool state);
bool relay_getState(uint8_t relayIndex);
void relay_addListener(RelayStateListener listener);
