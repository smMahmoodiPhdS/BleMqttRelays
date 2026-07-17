#pragma once
#include <Arduino.h>

// TODO: adjust to your actual model code / hardware revision naming.
#define OTA_MODEL_CODE "blemqttrelay"
#define OTA_HARDWARE_VERSION "1-0"

void setup_ota();
void loop_ota();

// MQTT topic the device listens on to learn the latest available version.
// Not farm-owner-scoped: it's a fleet-wide "what's the newest firmware for
// this model+hardware" topic, same convention as the old project.
String ota_versionTopic();
void ota_setRemoteVersion(const String& version);
