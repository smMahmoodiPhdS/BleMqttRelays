#include "mqttFunctions.h"
#include "wifiManagerTools.h"
#include "relayManager.h"
#include "otaUpdater.h"
#include <WiFi.h>

// TODO: set your MQTT broker host/port/credentials.
#define MQTT_SERVER "87.107.165.201"
#define MQTT_PORT 1883
#define MQTT_USER "viewers"
#define MQTT_PASSWORD "1234zxcV@"
//int res = iranMqttClient.connect(clientId.c_str(), "", "");

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

static String relayTopic(uint8_t relayNumOneBased, const char* suffix) {
    return "actuator/" + String(farmOwner) + "/rl0" + String(relayNumOneBased) + "/" + suffix;
}

// Device presence topic (retained). Uses the same traffic-light-friendly values
// as relays: 2 = online, 1 = offline. The broker publishes "1" here via the
// Last-Will when the device drops; the device publishes "2" right after connect.
static String presenceTopic() {
    return "actuator/" + String(farmOwner) + "/online";
}

void mqtt_publishRelayState(uint8_t relayIndex, bool state) {
    String topic = relayTopic(relayIndex + 1, "state");
    // Convert internal bool -> broker value (1 = off, 2 = on).
    mqttClient.publish(topic.c_str(), state ? RELAY_MQTT_ON : RELAY_MQTT_OFF, true);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);

    // Debug: log every inbound MQTT message (topic + payload) on the serial
    // monitor. Useful for verifying that the app's publishes actually reach the
    // board — e.g. relay commands, and (with the sensor subscription below) the
    // temperature limits sensors/<owner>/ts1/ll|ul.
    String rxMsg;
    for (unsigned int i = 0; i < length; i++) rxMsg += (char)payload[i];
    Serial.print("[MQTT RX] ");
    Serial.print(topicStr);
    Serial.print(" = ");
    Serial.println(rxMsg);

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        if (topicStr == relayTopic(i + 1, "on")) {
            relay_setState(i, true);
            return;
        }
        if (topicStr == relayTopic(i + 1, "off")) {
            relay_setState(i, false);
            return;
        }
    }

    if (topicStr == ota_versionTopic()) {
        String versionMsg;
        for (unsigned int i = 0; i < length; i++) versionMsg += (char)payload[i];
        ota_setRemoteVersion(versionMsg);
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

    String clientId = String("BleMqttRelay-") + farmOwner + "-" + String(random(0xffff), HEX);
    String willTopic = presenceTopic();
    // Last-Will: if the device drops, the broker retains "1" (offline) here.
    // Args: clientId, user, pass, willTopic, willQoS, willRetain, willMessage.
    if (!mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                            willTopic.c_str(), 0, true, "1")) return false;

    // Announce presence (retained "2" = online) now that we're connected.
    mqttClient.publish(willTopic.c_str(), "2", true);

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        mqttClient.subscribe(relayTopic(i + 1, "on").c_str());
        mqttClient.subscribe(relayTopic(i + 1, "off").c_str());
    }
    mqttClient.subscribe(ota_versionTopic().c_str());

    // --- Groundwork for on-board heater/cooler control (see reconciliation doc,
    // "Heater/cooler control" section) ---
    // The relay firmware historically did NOT subscribe to the sensor stream, so
    // the app's sensors/<owner>/ts1/ll|ul limit updates never reached the board.
    // Subscribe to temperature sensor 1 so those values arrive and show up on the
    // serial monitor. For now they are only logged (by mqttCallback); the
    // hysteresis heater/cooler logic from the original multiActuator firmware is
    // still to be ported. Covers ts1/cu, ts1/ll, ts1/ul.
    {
        String tsTopic = String("sensors/") + farmOwner + "/ts1/#";
        mqttClient.subscribe(tsTopic.c_str());
    }

    // Republish current state on every (re)connect so a broker/dashboard that
    // just restarted still gets it, even though it's also retained.
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        mqtt_publishRelayState(i, relay_getState(i));
    }
    return true;
}

// --- TEMPORARY sensor simulator (bench testing) -----------------------------
// The relay firmware publishes no sensor data, so nothing emits ts1/cu — and the
// app only shows a temperature card once a *current value* arrives (it gates the
// card on updateMillis, which is set only by ts1/cu). This publishes a fake
// current temperature to sensors/<owner>/ts1/cu so the card appears and the
// full loop (card -> +/- -> ll/ul back to the board) can be exercised without
// the real on-farm sensor board. Set SIMULATE_TS1 = false to disable.
static const bool SIMULATE_TS1 = true;
static const unsigned long SIM_INTERVAL_MS = 5000;
static unsigned long lastSimMs = 0;

static void mqtt_simulateSensors() {
    if (!SIMULATE_TS1) return;
    unsigned long now = millis();
    if (now - lastSimMs < SIM_INTERVAL_MS) return;
    lastSimMs = now;

    // Triangle wave 24.0 .. 30.0 .. 24.0 to exercise heater/cooler thresholds.
    static float simTemp = 24.0f;
    static float simDir = 0.5f;
    simTemp += simDir;
    if (simTemp >= 30.0f) { simTemp = 30.0f; simDir = -0.5f; }
    if (simTemp <= 24.0f) { simTemp = 24.0f; simDir = 0.5f; }

    String topic = String("sensors/") + farmOwner + "/ts1/cu";
    String payload = String(simTemp, 1);
    // Non-retained, like a real live sensor reading.
    mqttClient.publish(topic.c_str(), payload.c_str(), false);
    Serial.print("[SIM TX] ");
    Serial.print(topic);
    Serial.print(" = ");
    Serial.println(payload);
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
