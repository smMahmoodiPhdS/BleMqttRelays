#include "wifiManagerTools.h"
#include <string.h>

DNSServer dnsServer;
WebServer server(80);
IotWebConf iotWebConf(DEVICE_NAME_PREFIX, &dnsServer, &server, WIFI_AP_PASSWORD);

// ---------------------------------------------------------------------------
// Provisioned values. All empty by default — deliberately.
//
// The previous build defaulted farmOwner to "demo" so a fresh board would join
// a default namespace and appear to work. On the new broker that is the worst
// possible default: "demo" has no ACL entry, so every publish is refused and
// the board looks dead rather than unconfigured. Empty plus an explicit refusal
// (see config_isComplete) turns a silent failure into a visible one.
// ---------------------------------------------------------------------------
char farmOwner[CFG_NAME_LEN]      = "";
char farmId[CFG_NAME_LEN]         = "";
char mqttHost[CFG_HOST_LEN]       = "link.asanautomation.com";
char mqttPort[6]                  = "8883";
char mqttUser[CFG_USER_LEN]       = "";
char mqttPassword[CFG_PASS_LEN]   = "";

static IotWebConfParameterGroup identityGroup =
    IotWebConfParameterGroup("identity", "شناسه مزرعه / Farm identity");
static IotWebConfParameterGroup brokerGroup =
    IotWebConfParameterGroup("broker", "کارگزار / MQTT broker");

static IotWebConfTextParameter farmOwnerParam =
    IotWebConfTextParameter("نام مالک (farmOwner)", "farmOwner", farmOwner, CFG_NAME_LEN);
static IotWebConfTextParameter farmIdParam =
    IotWebConfTextParameter("شناسه مزرعه (farmId)", "farmId", farmId, CFG_NAME_LEN);
static IotWebConfTextParameter mqttHostParam =
    IotWebConfTextParameter("آدرس کارگزار", "mqttHost", mqttHost, CFG_HOST_LEN);
static IotWebConfNumberParameter mqttPortParam =
    IotWebConfNumberParameter("پورت", "mqttPort", mqttPort, sizeof(mqttPort), "8883");
static IotWebConfTextParameter mqttUserParam =
    IotWebConfTextParameter("نام کاربری", "mqttUser", mqttUser, CFG_USER_LEN);
static IotWebConfPasswordParameter mqttPasswordParam =
    IotWebConfPasswordParameter("رمز عبور", "mqttPass", mqttPassword, CFG_PASS_LEN);

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool config_nameIsLegal(const char* value) {
    if (value == nullptr) return false;
    size_t len = strlen(value);
    if (len == 0 || len > 64) return false;
    // Must start alphanumeric; '-' allowed thereafter. Lowercase only: MQTT
    // topic levels are case-sensitive, so permitting mixed case would let
    // "Farm01" and "farm01" exist as two different farms with no error.
    for (size_t i = 0; i < len; i++) {
        char c = value[i];
        bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (i == 0 && !alnum) return false;
        if (!alnum && c != '-') return false;
    }
    return true;
}

static bool hostIsPlausible(const char* h) {
    size_t len = strlen(h);
    if (len == 0 || len >= CFG_HOST_LEN) return false;
    for (size_t i = 0; i < len; i++) {
        char c = h[i];
        if (c == ' ' || c == '/' || c == '#' || c == '+') return false;
    }
    return true;
}

bool config_isComplete() {
    return config_problem() == nullptr;
}

const char* config_problem() {
    if (!config_nameIsLegal(farmOwner))
        return "farmOwner is empty or illegal (1-64 chars of a-z 0-9 -, lowercase)";
    if (!config_nameIsLegal(farmId))
        return "farmId is empty or illegal (1-64 chars of a-z 0-9 -, lowercase)";
    if (!hostIsPlausible(mqttHost))
        return "broker host is empty or contains an illegal character";
    if (strlen(mqttUser) == 0)
        return "MQTT username is empty";
    if (strlen(mqttPassword) == 0)
        return "MQTT password is empty";
    return nullptr;
}

// ---------------------------------------------------------------------------
// Portal
// ---------------------------------------------------------------------------

static void handleRoot() {
    if (iotWebConf.handleCaptivePortal()) return;

    const char* problem = config_problem();
    String s = "<!DOCTYPE html><html lang=\"fa\"><head>"
               "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>"
               "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"
               "<title>ASN actuator</title></head>"
               "<body style='font-family:tahoma; font-size:14px; direction:rtl; text-align:center;'>"
               "<p><a href='config'>تنظیمات / Settings</a></p><hr>";

    if (problem == nullptr) {
        s += "<p style='color:green'>پیکربندی کامل است / Configured</p>";
        s += "<p style='direction:ltr'><code>";
        s += String(farmOwner) + "/" + String(farmId);
        s += "</code><br><code>";
        s += String(mqttUser) + " @ " + String(mqttHost) + ":" + String(mqttPort);
        s += "</code></p>";
    } else {
        s += "<p style='color:#c0392b'>پیکربندی ناقص / Not configured</p>";
        s += "<p style='direction:ltr'><code>";
        s += problem;
        s += "</code></p>";
    }
    s += "</body></html>";
    server.send(200, "text/html", s);
}

static void configSaved() {
    Serial.println("[config] saved");
    const char* problem = config_problem();
    if (problem) {
        Serial.printf("[config] still incomplete: %s\n", problem);
    } else {
        Serial.printf("[config] identity %s/%s, broker %s:%s as %s\n",
                      farmOwner, farmId, mqttHost, mqttPort, mqttUser);
        // Restart so the MQTT layer picks up the new values cleanly rather than
        // trying to re-point a live connection.
        Serial.println("[config] restarting to apply");
        delay(500);
        ESP.restart();
    }
}

static bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper) {
    bool ok = true;

    // Reject rather than sanitise. Silently rewriting someone's farm name puts
    // the board in a namespace they did not choose, which is worse than making
    // them retype it.
    String owner = webRequestWrapper->arg(farmOwnerParam.getId());
    if (!config_nameIsLegal(owner.c_str())) {
        farmOwnerParam.errorMessage = "1-64 chars: a-z, 0-9, '-'. Lowercase, no dots or spaces.";
        ok = false;
    }
    String fid = webRequestWrapper->arg(farmIdParam.getId());
    if (!config_nameIsLegal(fid.c_str())) {
        farmIdParam.errorMessage = "1-64 chars: a-z, 0-9, '-'. Lowercase, no dots or spaces.";
        ok = false;
    }
    String host = webRequestWrapper->arg(mqttHostParam.getId());
    if (!hostIsPlausible(host.c_str())) {
        mqttHostParam.errorMessage = "Broker hostname required, no spaces or / # +";
        ok = false;
    }
    String user = webRequestWrapper->arg(mqttUserParam.getId());
    if (user.length() == 0) {
        mqttUserParam.errorMessage = "Required — the device account from credentials.txt";
        ok = false;
    }
    return ok;
}

void resetWifiManager() {
    iotWebConf.resetWifiAuthInfo();
    ESP.restart();
}

void setup_wifiManager() {
    identityGroup.addItem(&farmOwnerParam);
    identityGroup.addItem(&farmIdParam);
    brokerGroup.addItem(&mqttHostParam);
    brokerGroup.addItem(&mqttPortParam);
    brokerGroup.addItem(&mqttUserParam);
    brokerGroup.addItem(&mqttPasswordParam);

    iotWebConf.addParameterGroup(&identityGroup);
    iotWebConf.addParameterGroup(&brokerGroup);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setFormValidator(&formValidator);
    iotWebConf.init();

    server.on("/", handleRoot);
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    const char* problem = config_problem();
    if (problem) {
        Serial.printf("\n[config] NOT CONFIGURED: %s\n", problem);
        Serial.println("[config] MQTT will not be attempted. Open the captive portal.");
    } else {
        Serial.printf("[config] %s/%s -> %s:%s as %s\n",
                      farmOwner, farmId, mqttHost, mqttPort, mqttUser);
    }
}

void loop_wifiManager() {
    iotWebConf.doLoop();
}
