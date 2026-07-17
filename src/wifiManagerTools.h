#pragma once

#include <DNSServer.h>
#include <WebServer.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

#define WIFI_AP_PASSWORD "12345678"
#define DEVICE_NAME_PREFIX "BleMqttRelay"

extern DNSServer dnsServer;
extern WebServer server;
extern IotWebConf iotWebConf;

// Farm/customer name, set via the captive portal. Used both as an MQTT topic
// segment (actuator/<farmOwner>/rlNN/...) and as a BLE identity component.
extern char farmOwner[128];

void setup_wifiManager();
void loop_wifiManager();
void resetWifiManager();
