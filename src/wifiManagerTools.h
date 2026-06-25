#pragma once

#include <IotWebConf.h>
#include <DNSServer.h>
#include <WebServer.h>

extern DNSServer dnsServer;
extern WebServer server;
extern IotWebConf iotWebConf;

void setup_wifiManager();
void loop_wifiManager();
