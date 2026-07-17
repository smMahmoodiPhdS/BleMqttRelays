#include "wifiManagerTools.h"

DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(DEVICE_NAME_PREFIX, &dnsServer, &server, WIFI_AP_PASSWORD);

// Default identity. Kept as "demo" so a freshly-flashed device joins the same
// default namespace the mobile app uses out of the box (default auth "demo"):
// actuator/demo/rl0N/... — set the real farm owner via the captive portal.
char farmOwner[128] = "demo";
IotWebConfTextParameter farmOwnerParam =
    IotWebConfTextParameter("نام مزرعه / مشتری", "farmOwner", farmOwner, sizeof(farmOwner));

static void handleRoot() {
    if (iotWebConf.handleCaptivePortal()) return;

    String s = "<!DOCTYPE html><html lang=\"fa\"><head>"
               "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>"
               "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"
               "<title>BleMqttRelays</title></head>"
               "<body style='font-family:tahoma; font-size:14px; direction:rtl; text-align:center; color:blue;'>"
               "برای انجام تنظیمات، <a href='config'>اینجا</a> کلیک کنید"
               "<p>نام ذخیره شده: ";
    s += farmOwner;
    s += "</p></body></html>";
    server.send(200, "text/html", s);
}

static void configSaved() {
    Serial.println("Configuration was updated.");
}

static bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {
    return true;
}

void resetWifiManager() {
    iotWebConf.resetWifiAuthInfo();
    ESP.restart();
}

void setup_wifiManager() {
    iotWebConf.addSystemParameter(&farmOwnerParam);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setFormValidator(&formValidator);
    iotWebConf.init();

    server.on("/", handleRoot);
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });
}

void loop_wifiManager() {
    iotWebConf.doLoop();
}
