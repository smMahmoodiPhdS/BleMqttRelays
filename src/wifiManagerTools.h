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

// Field sizes. farmOwner and farmId are topic levels AND components of the
// broker account name, so they follow the 64-char rule from
// Docs/Architecture/Namespace-Cutover-Readiness.md §2.3.
#define CFG_NAME_LEN   65     // 64 chars + NUL
#define CFG_HOST_LEN   96
#define CFG_USER_LEN   96
#define CFG_PASS_LEN   65

extern DNSServer dnsServer;
extern WebServer server;
extern IotWebConf iotWebConf;

// ---------------------------------------------------------------------------
// Provisioned identity and broker settings, all from the captive portal.
//
// Nothing here is a #define any more. The old build hardcoded the broker IP and
// a shared account into the binary, which meant every board carried the same
// credential and a server move needed a reflash.
// ---------------------------------------------------------------------------
extern char farmOwner[CFG_NAME_LEN];   // topic level 2, e.g. "smmahmoodi"
extern char farmId[CFG_NAME_LEN];      // topic level 3, e.g. "farm01"
extern char mqttHost[CFG_HOST_LEN];    // e.g. "link.asanautomation.com"
extern char mqttPort[6];               // 8883
extern char mqttUser[CFG_USER_LEN];    // dev.<owner>.<farm>.<module>
extern char mqttPassword[CFG_PASS_LEN];

void setup_wifiManager();
void loop_wifiManager();
void resetWifiManager();

// True only when every field needed to reach the broker is present and legal.
// The firmware refuses to attempt MQTT until this holds — a board that quietly
// publishes into the wrong namespace is indistinguishable from a dead one.
bool config_isComplete();

// Why config_isComplete() is false, for the log and the portal.
const char* config_problem();

// Validation shared by the portal form and the boot check. Rejects anything
// that would corrupt a topic or an ACL entry: '.', '/', '+', '#', whitespace,
// uppercase, or an empty value. '+' and '#' matter most — either one in a farm
// name silently widens every ACL rule from one farm to all of them.
bool config_nameIsLegal(const char* value);
