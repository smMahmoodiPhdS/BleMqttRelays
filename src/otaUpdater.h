#pragma once
#include <Arduino.h>

#include "boardConfig.h"

#define OTA_MODEL_CODE "blemqttrelay"

// Host serving the firmware binaries. Must present a Let's Encrypt certificate,
// because the download is now HTTPS validated against the pinned root - see the
// note in otaUpdater.cpp about why plaintext OTA is the worst plaintext channel
// in a system whose devices flash what they fetch.
#define OTA_HOST "asanautomation.com"

// The hardware version is part of the OTA topic and the binary filename, so the
// two board variants can never pull each other's firmware. That matters more
// than it looks: the pin maps overlap in hostile ways, and a protoboard binary
// landing on the actuator PCB would drive relay outputs into DIP pull-ups and
// the status LED. Keep these distinct.
#if defined(BOARD_ACTUATOR_BLE)
#define OTA_HARDWARE_VERSION "2-0"
#else
#define OTA_HARDWARE_VERSION "1-0"
#endif

void setup_ota();
void loop_ota();

// MQTT topic the device listens on to learn the latest available version.
// Not farm-owner-scoped: it's a fleet-wide "what's the newest firmware for
// this model+hardware" topic, same convention as the old project.
String ota_versionTopic();
void ota_setRemoteVersion(const String& version);
