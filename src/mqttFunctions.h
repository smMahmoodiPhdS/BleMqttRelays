#pragma once
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "relayManager.h"

#define MQTT_RECONNECT_INTERVAL_MS 30000UL

// Shared with otaUpdater, which needs a client for HTTPUpdate. Now a TLS
// client, so OTA over https:// works with the same pinned root.
extern WiFiClientSecure wifiClient;
extern PubSubClient mqttClient;

void setup_mqtt();
void loop_mqtt();

// Publishes the full relay picture for one channel:
//   rl0N/state -> measured coil state  (1 = off, 2 = on)  [retained]
//   rl0N/cmd   -> last commanded state (1 = off, 2 = on)  [retained]
//   rl0N/mode  -> "auto" | "override_on" | "override_off" | "unknown" [retained]
void mqtt_publishRelayStatus(uint8_t relayIndex, const RelayStatus& status);

// Publish to the paired sensor of the current module: sensors/<owner>/<type><A>/<field>.
// Status uses the 1=off / 2=on convention (retained); value publishes a number.
void mqtt_publishSensorStatus(const char* sensorType, const char* field, bool on);
void mqtt_publishSensorValue(const char* sensorType, const char* field, float value, bool retain);
