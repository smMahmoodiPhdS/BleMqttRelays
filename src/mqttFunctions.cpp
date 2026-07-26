#include "mqttFunctions.h"
#include "wifiManagerTools.h"
#include "relayManager.h"
#include "otaUpdater.h"
#include "roleConfig.h"
#include "roleManager.h"
#include "sensorSim.h"
#include <WiFi.h>

// TODO: set your MQTT broker host/port/credentials.
#define MQTT_SERVER "87.107.165.201"
#define MQTT_PORT 1883
#define MQTT_USER "viewers"
#define MQTT_PASSWORD "1234zxcV@"

// Broker-facing relay value convention: 1 = OFF, 2 = ON (Grafana traffic-light
// can't key off 0). The firmware keeps pin state as a bool and converts only here.
#define RELAY_MQTT_OFF "1"
#define RELAY_MQTT_ON  "2"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Re-assert the retained relay topics on a slow cadence so a stale retained
// value can never outlive reality.
#define RELAY_STATE_HEARTBEAT_MS 60000UL

static unsigned long lastReconnectAttempt = 0;
static unsigned long lastRelayHeartbeat = 0;

// ---- Addressed topics: <prefix><A> from the function/address switches --------
static String moduleBase() {
    const RoleConfig& r = roleConfig_get();
    return "actuator/" + String(farmOwner) + "/" + r.topicPrefix +
           String(roleConfig_address()) + "/";
}
static String relayTopic(uint8_t relayNumOneBased, const char* suffix) {
    return moduleBase() + "rl0" + String(relayNumOneBased) + "/" + suffix;
}
static String presenceTopic() {
    return moduleBase() + "online";
}
static String functionTopic() {
    const RoleConfig& r = roleConfig_get();
    return "configs/" + String(farmOwner) + "/" + r.topicPrefix +
           String(roleConfig_address()) + "/function";
}
// sensors/<owner>/<type><A>/  — paired sensor of the current module (same index).
static String sensorBase(const char* sensorType) {
    return "sensors/" + String(farmOwner) + "/" + String(sensorType) +
           String(roleConfig_address()) + "/";
}

// `state` now carries the *measured* coil state from the feedback divider, not
// the commanded one, so Grafana and the app show what the relay is really doing
// even when the on-board slider is driving it. The commanded value moved to
// `cmd`, and `mode` says whether the two agree.
void mqtt_publishRelayStatus(uint8_t relayIndex, const RelayStatus& status) {
    if (!mqttClient.connected()) return;
    uint8_t n = relayIndex + 1;
    mqttClient.publish(relayTopic(n, "state").c_str(),
                       status.actual ? RELAY_MQTT_ON : RELAY_MQTT_OFF, true);
    mqttClient.publish(relayTopic(n, "cmd").c_str(),
                       status.commanded ? RELAY_MQTT_ON : RELAY_MQTT_OFF, true);
    mqttClient.publish(relayTopic(n, "mode").c_str(),
                       relay_modeName(status.mode), true);
}

void mqtt_publishSensorStatus(const char* sensorType, const char* field, bool on) {
    String topic = sensorBase(sensorType) + field;
    mqttClient.publish(topic.c_str(), on ? RELAY_MQTT_ON : RELAY_MQTT_OFF, true);
}

void mqtt_publishSensorValue(const char* sensorType, const char* field, float value, bool retain) {
    String topic = sensorBase(sensorType) + field;
    mqttClient.publish(topic.c_str(), String(value, 1).c_str(), retain);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    String rxMsg;
    for (unsigned int i = 0; i < length; i++) rxMsg += (char)payload[i];

    // Declutter the monitor: skip high-frequency / self-echo topics.
    bool noisy = topicStr.endsWith("/cu") || topicStr.endsWith("/hOn") ||
                 topicStr.endsWith("/cOn") || topicStr.endsWith("/daily_min") ||
                 topicStr.endsWith("/daily_max");
    if (!noisy) {
        Serial.print("[MQTT RX] ");
        Serial.print(topicStr);
        Serial.print(" = ");
        Serial.println(rxMsg);
    }

    // Route paired-sensor values (sensors/<owner>/<type><A>/<field>) to the role.
    {
        String sbase = String("sensors/") + farmOwner + "/";
        if (topicStr.startsWith(sbase)) {
            String rest = topicStr.substring(sbase.length());   // "ts1/cu"
            int slash = rest.indexOf('/');
            if (slash > 0) {
                String node = rest.substring(0, slash);          // "ts1"
                String field = rest.substring(slash + 1);        // "cu"
                String addrStr = String(roleConfig_address());
                if (node.endsWith(addrStr)) {
                    String type = node.substring(0, node.length() - addrStr.length());  // "ts"
                    roleManager_onSensor(type, field, rxMsg.toFloat());
                }
            }
            return;
        }
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
    relay_addListener(mqtt_publishRelayStatus);
}

static bool mqtt_reconnect() {
    if (mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    String clientId = String("BleMqttRelay-") + farmOwner + "-" +
                      roleConfig_get().topicPrefix + String(roleConfig_address()) +
                      "-" + String(random(0xffff), HEX);
    String willTopic = presenceTopic();
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

    // Subscribe to whichever paired sensor streams the active role consumes.
    if (roleUsesTemperature()) mqttClient.subscribe((sensorBase("ts") + "#").c_str());
    if (roleUsesHumidity())    mqttClient.subscribe((sensorBase("hs") + "#").c_str());
    if (roleUsesLight())       mqttClient.subscribe((sensorBase("ls") + "#").c_str());

    // Let the role publish its initial status, then republish relay states.
    roleManager_onConnect();
    relay_republishAll();
    lastRelayHeartbeat = millis();
    return true;
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

    // The relay topics are retained, so a broker restart or a publish lost
    // during a flaky link would leave a stale value sitting there claiming a
    // heater is on. Re-assert the truth periodically - it is three small
    // retained publishes per channel per minute.
    unsigned long now = millis();
    if (now - lastRelayHeartbeat >= RELAY_STATE_HEARTBEAT_MS) {
        lastRelayHeartbeat = now;
        relay_republishAll();
    }

    sensorSim_loop();
}
