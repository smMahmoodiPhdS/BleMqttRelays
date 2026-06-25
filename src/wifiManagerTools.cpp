#include "wifiManagerTools.h"
#include <helpers.h>

DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf("BleMqttRelays", &dnsServer, &server, "blemqttrelay");

void setup_wifiManager() {
  iotWebConf.init();
  server.on("/", []() {
    if (iotWebConf.handleCaptivePortal()) return;
    server.send(200, "text/html", "<h1>BleMqttRelays</h1><p>Use the config page to set WiFi.</p>");
  });
  server.on("/config", []() { iotWebConf.handleConfig(); });
  server.onNotFound([]() { iotWebConf.handleNotFound(); });
}

void loop_wifiManager() {
  iotWebConf.doLoop();
}
