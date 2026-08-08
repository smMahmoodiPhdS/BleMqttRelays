#include "Arduino.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include "HTTPUpdate.h"
SerialCls Serial;
WiFiCls WiFi;
HttpUpdateCls httpUpdate;
int g_dipLevel[40];
unsigned long g_millis = 1000;
std::vector<PubRec> g_pubs;
std::vector<std::string> g_subs;
bool g_connected = false, g_connectOk = true;
std::string g_willTopic, g_willPayload, g_clientId, g_user;
