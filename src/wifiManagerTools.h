#pragma once

#include <DNSServer.h>
#include <WebServer.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

#define WIFI_AP_PASSWORD "12345678"

// BLE advertised name prefix. Short and neutral on purpose:
//
//   * Byte budget. The advertised name lives in the 31-octet SCAN_RSP packet,
//     minus 2 bytes of AD overhead = 29 characters total. "BleMqttRelay-" ate
//     13 of them; "ASN-" costs 4, leaving 25. See the arithmetic in
//     Docs/Architecture/Namespace-Cutover-Readiness.md §2.4 — and note that 25
//     still does not satisfy the no-truncation rule, so the full identity has
//     to move to a GATT characteristic regardless. This is headroom, not a fix.
//
//   * Disclosure. "BleMqttRelay" told anyone within radio range exactly which
//     protocol stack and device class they were looking at, which is free
//     reconnaissance. A neutral token does not.
#define DEVICE_NAME_PREFIX "ASN"

extern DNSServer dnsServer;
extern WebServer server;
extern IotWebConf iotWebConf;

// Farm/customer name, set via the captive portal. Used both as an MQTT topic
// segment (actuator/<farmOwner>/rlNN/...) and as a BLE identity component.
extern char farmOwner[128];

void setup_wifiManager();
void loop_wifiManager();
void resetWifiManager();
