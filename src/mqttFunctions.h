#pragma once
#include <PubSubClient.h>
#include <WiFiClient.h>

#define MQTT_RECONNECT_INTERVAL_MS 30000UL

// Shared with otaUpdater, which needs a WiFiClient for HTTPUpdate.
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

void setup_mqtt();
void loop_mqtt();
void mqtt_publishRelayState(uint8_t relayIndex, bool state);
