#include "mqttFunctions.h"
#include "wifiManagerTools.h"
#include "relayManager.h"
#include "otaUpdater.h"
#include "roleConfig.h"
#include "roleManager.h"
#include "sensorSim.h"
#include "netTime.h"
#include "rootCA.h"
#include <WiFi.h>

// Broker host, port and credentials all come from the captive portal now
// (wifiManagerTools). They used to be #defines, which meant every board carried
// the same shared account and moving the server required a reflash.

// Broker-facing relay value convention: 1 = OFF, 2 = ON (Grafana traffic-light
// can't key off 0). The firmware keeps pin state as a bool and converts only here.
#define RELAY_MQTT_OFF "1"
#define RELAY_MQTT_ON  "2"

// TLS everywhere. The broker publishes only 8883; 1883 exists solely on the
// server's internal Docker network and is not reachable from a device.
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

static bool tlsConfigured = false;

// Re-assert the retained relay topics on a slow cadence so a stale retained
// value can never outlive reality.
#define RELAY_STATE_HEARTBEAT_MS 60000UL

static unsigned long lastReconnectAttempt = 0;
static unsigned long lastRelayHeartbeat = 0;

// The first connect attempt is immediate; MQTT_RECONNECT_INTERVAL_MS applies only
// to RETRIES after a real rejection.
//
// Without this flag, `millis() - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL_MS`
// is false for the first 30 seconds of uptime (because lastReconnectAttempt starts
// at 0), so a perfectly healthy board sits idle for half a minute before it even
// tries. Stacked on the NTP wait, first publish lands ~35 s after boot — which on
// the bench is indistinguishable from a board that is not working.
static bool everAttempted = false;

// ---------------------------------------------------------------------------
// Topic construction. EVERY topic in this file goes through farmScope(), so the
// namespace shape is defined in exactly one place.
//
// Option A: the farm gets its own level.
//   before:  sensors/<owner>/ts1/cu
//   now:     sensors/<owner>/<farm>/ts1/cu
//
// See Docs/Architecture/APP-AND-CONTRACT.md §1.
// ---------------------------------------------------------------------------
static String farmScope() {
    return String(farmOwner) + "/" + String(farmId);
}

// ---- Addressed topics: <prefix><A> from the function/address switches --------
static String moduleBase() {
    const RoleConfig& r = roleConfig_get();
    return "actuator/" + farmScope() + "/" + r.topicPrefix +
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
    return "configs/" + farmScope() + "/" + r.topicPrefix +
           String(roleConfig_address()) + "/function";
}
// sensors/<owner>/<type><A>/  — paired sensor of the current module (same index).
static String sensorBase(const char* sensorType) {
    return "sensors/" + farmScope() + "/" + String(sensorType) +
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
        // Uses farmScope() too. This one is easy to miss because it builds a
        // prefix for startsWith() rather than a topic to publish — leave it on
        // the old shape and inbound sensor values are silently ignored while
        // outbound publishing looks perfect.
        String sbase = String("sensors/") + farmScope() + "/";
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
    mqttClient.setCallback(mqttCallback);
    relay_addListener(mqtt_publishRelayStatus);
    // Server and TLS are configured lazily in mqtt_reconnect(), once the clock
    // is sane — setting a CA before the time is known just fails later.
}

// Three outcomes, not two — and the distinction matters for the retry timer.
//
// NotReady means we never touched the network: no WiFi, no configuration, or no
// sane clock yet. Those are pre-flight checks, and burning a 30-second retry
// interval on one is wrong. A board that boots a few seconds before NTP lands
// would otherwise fail its first attempt on the clock gate and then sit idle for
// half a minute after the clock had become perfectly good.
//
// Failed means the broker actually rejected us — bad credentials, TLS refused,
// host unreachable. That earns the backoff.
enum class ConnectOutcome { NotReady, Failed, Connected };

static ConnectOutcome mqtt_reconnect() {
    if (mqttClient.connected()) return ConnectOutcome::Connected;
    if (WiFi.status() != WL_CONNECTED) return ConnectOutcome::NotReady;

    // Gate 1: configuration. A board with no farmId would publish into a
    // namespace the ACL does not grant, and every publish would be refused —
    // which looks exactly like a dead board.
    if (!config_isComplete()) {
        static unsigned long lastNag = 0;
        if (millis() - lastNag > 30000) {
            lastNag = millis();
            Serial.printf("[mqtt] not attempting: %s\n", config_problem());
        }
        return ConnectOutcome::NotReady;
    }

    // Gate 2: the clock. TLS validates notBefore/notAfter against it, and an
    // ESP32 boots in 1970, so this must hold before the first handshake.
    if (!netTime_isSynced()) {
        static unsigned long lastNag = 0;
        if (millis() - lastNag > 30000) {
            lastNag = millis();
            Serial.println("[mqtt] waiting for NTP — TLS cannot validate a certificate yet");
        }
        return ConnectOutcome::NotReady;
    }

    if (!tlsConfigured) {
        wifiClient.setCACert(ISRG_ROOT_X1);
        mqttClient.setServer(mqttHost, (uint16_t)atoi(mqttPort));
        tlsConfigured = true;
        Serial.printf("[mqtt] TLS armed for %s:%s, clock %s\n",
                      mqttHost, mqttPort, netTime_iso8601().c_str());
    }

    String clientId = String("asn-") + farmOwner + "-" + farmId + "-" +
                      roleConfig_get().topicPrefix + String(roleConfig_address()) +
                      "-" + String(random(0xffff), HEX);
    String willTopic = presenceTopic();
    if (!mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword,
                            willTopic.c_str(), 0, true, "1")) {
        // PubSubClient state codes: -2 is a transport failure, which on a TLS
        // link almost always means the handshake was rejected rather than the
        // credentials.
        int st = mqttClient.state();
        Serial.printf("[mqtt] connect failed, state=%d%s\n", st,
                      st == -2 ? " (transport/TLS — check the clock and the CA)" : "");
        return ConnectOutcome::Failed;
    }

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
    return ConnectOutcome::Connected;
}

void loop_mqtt() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (everAttempted && (now - lastReconnectAttempt) < MQTT_RECONNECT_INTERVAL_MS) {
            return;   // still backing off from a real rejection
        }
        // The gates inside cost nothing and nag on timers of their own, so it is
        // safe to call this every loop while they are unsatisfied.
        ConnectOutcome outcome = mqtt_reconnect();
        if (outcome != ConnectOutcome::NotReady) {
            // Only a real attempt arms the backoff. A pre-flight refusal does not,
            // so the board connects the instant the clock lands rather than up to
            // 30 s later.
            everAttempted = true;
            lastReconnectAttempt = now;
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
