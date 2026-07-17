#include "mqttFunctions.h"
#include "wifiManagerTools.h"
#include "relayManager.h"
#include "otaUpdater.h"
#include "heaterControl.h"
#include "roleConfig.h"
#include <WiFi.h>

// TODO: set your MQTT broker host/port/credentials.
#define MQTT_SERVER "87.107.165.201"
#define MQTT_PORT 1883
#define MQTT_USER "viewers"
#define MQTT_PASSWORD "1234zxcV@"

// Broker-facing relay value convention. Grafana's traffic-light widget can't
// key off 0, so relay values on the broker use 1 = OFF, 2 = ON. The firmware
// keeps the pin/logical state as a plain bool (0/1) internally and converts
// only at the MQTT edge:
//   pin/bool -> MQTT value : false -> "1", true -> "2"
//   MQTT value -> pin/bool : "1" -> false (1->0), "2" -> true (2->1)
#define RELAY_MQTT_OFF "1"
#define RELAY_MQTT_ON  "2"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static unsigned long lastReconnectAttempt = 0;

// ---- Addressed topics: actuator/<owner>/<prefix><A>/... --------------------
// prefix (rmhc/rmlt/…) and address (1-based) come from the function+address
// switches via roleConfig. So multiple modules on one farm never collide.
static String moduleBase() {
    const RoleConfig& r = roleConfig_get();
    return "actuator/" + String(farmOwner) + "/" + r.topicPrefix +
           String(roleConfig_address()) + "/";
}

static String relayTopic(uint8_t relayNumOneBased, const char* suffix) {
    return moduleBase() + "rl0" + String(relayNumOneBased) + "/" + suffix;
}

// Per-module presence (retained). 2 = online, 1 = offline (via Last-Will).
static String presenceTopic() {
    return moduleBase() + "online";
}

// Retained descriptor so the app/Node-RED can discover each module's role.
static String functionTopic() {
    const RoleConfig& r = roleConfig_get();
    return "configs/" + String(farmOwner) + "/" + r.topicPrefix +
           String(roleConfig_address()) + "/function";
}

// Paired sensor base for the current role (same-index). Heater/cooler follows
// ts<A>; other roles will extend this (ls<A>/hs<A>/…) when implemented.
static String pairedSensorBase() {
    return "sensors/" + String(farmOwner) + "/ts" + String(roleConfig_address()) + "/";
}

void mqtt_publishRelayState(uint8_t relayIndex, bool state) {
    String topic = relayTopic(relayIndex + 1, "state");
    mqttClient.publish(topic.c_str(), state ? RELAY_MQTT_ON : RELAY_MQTT_OFF, true);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);

    String rxMsg;
    for (unsigned int i = 0; i < length; i++) rxMsg += (char)payload[i];

    // Declutter the serial monitor: skip high-frequency / self-echo topics.
    bool noisy = topicStr.endsWith("/cu") || topicStr.endsWith("/hOn") ||
                 topicStr.endsWith("/cOn") || topicStr.endsWith("/daily_min") ||
                 topicStr.endsWith("/daily_max");
    if (!noisy) {
        Serial.print("[MQTT RX] ");
        Serial.print(topicStr);
        Serial.print(" = ");
        Serial.println(rxMsg);
    }

    // Feed heater/cooler control with the paired temperature sensor's values.
    if (roleConfig_get().roleClass == ROLE_HEATER_COOLER) {
        String base = pairedSensorBase();
        if (topicStr == base + "cu") { heaterControl_setCurrent(rxMsg.toFloat()); return; }
        if (topicStr == base + "ll") { heaterControl_setLowerLimit(rxMsg.toFloat()); return; }
        if (topicStr == base + "ul") { heaterControl_setUpperLimit(rxMsg.toFloat()); return; }
    }

    // Relay commands (addressed) — same for every role.
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        if (topicStr == relayTopic(i + 1, "on"))  { relay_setState(i, true);  return; }
        if (topicStr == relayTopic(i + 1, "off")) { relay_setState(i, false); return; }
    }

    if (topicStr == ota_versionTopic()) {
        ota_setRemoteVersion(rxMsg);
    }
}

void setup_mqtt() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    relay_addListener(mqtt_publishRelayState);
}

static bool mqtt_reconnect() {
    if (mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    String clientId = String("BleMqttRelay-") + farmOwner + "-" +
                      roleConfig_get().topicPrefix + String(roleConfig_address()) +
                      "-" + String(random(0xffff), HEX);
    String willTopic = presenceTopic();
    // Last-Will: if the module drops, the broker retains "1" (offline) here.
    if (!mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                            willTopic.c_str(), 0, true, "1")) return false;

    // Presence (retained "2" = online) + role descriptor.
    mqttClient.publish(willTopic.c_str(), "2", true);
    mqttClient.publish(functionTopic().c_str(), roleConfig_get().fnName, true);

    // Relay commands (addressed).
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        mqttClient.subscribe(relayTopic(i + 1, "on").c_str());
        mqttClient.subscribe(relayTopic(i + 1, "off").c_str());
    }
    mqttClient.subscribe(ota_versionTopic().c_str());

    // Heater/cooler role: subscribe to its paired temperature sensor stream
    // (cu/ll/ul) so the control loop and app limit updates reach the board.
    if (roleConfig_get().roleClass == ROLE_HEATER_COOLER) {
        String tsTopic = pairedSensorBase() + "#";
        mqttClient.subscribe(tsTopic.c_str());
    }

    // Republish current relay state on every (re)connect.
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        mqtt_publishRelayState(i, relay_getState(i));
    }
    return true;
}

// --- TEMPORARY sensor simulator (bench testing) -----------------------------
// Publishes a fake current temperature to the board's paired sensor
// (sensors/<owner>/ts<A>/cu) so the app card appears and the heater/cooler loop
// can be exercised without the real sensor board. Only meaningful for the
// heater/cooler role. Set SIMULATE_TS1 = false to disable.
static const bool SIMULATE_TS1 = true;
static const unsigned long SIM_INTERVAL_MS = 5000;
static unsigned long lastSimMs = 0;

static void mqtt_simulateSensors() {
    if (!SIMULATE_TS1) return;
    if (roleConfig_get().roleClass != ROLE_HEATER_COOLER) return;
    unsigned long now = millis();
    if (now - lastSimMs < SIM_INTERVAL_MS) return;
    lastSimMs = now;

    // Triangle wave 24.0 .. 30.0 .. 24.0 to exercise heater/cooler thresholds.
    static float simTemp = 24.0f;
    static float simDir = 0.5f;
    simTemp += simDir;
    if (simTemp >= 30.0f) { simTemp = 30.0f; simDir = -0.5f; }
    if (simTemp <= 24.0f) { simTemp = 24.0f; simDir = 0.5f; }

    String topic = pairedSensorBase() + "cu";
    String payload = String(simTemp, 1);
    mqttClient.publish(topic.c_str(), payload.c_str(), false);
}

void loop_mqtt() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            mqtt_reconnect();
        }
        return;
    }
    mqttClient.loop();
    mqtt_simulateSensors();
}
