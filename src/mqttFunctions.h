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

// Publish to the paired sensor of the current module: sensors/<owner>/<type><A>/<field>.
// Status uses the 1=off / 2=on convention (retained); value publishes a number.
void mqtt_publishSensorStatus(const char* sensorType, const char* field, bool on);
void mqtt_publishSensorValue(const char* sensorType, const char* field, float value, bool retain);
