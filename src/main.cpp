#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "wifiManagerTools.h"

#define RELAY_COUNT 4
const int relayPins[RELAY_COUNT] = {16, 17, 18, 19};

const char* mqttServer = "192.168.1.100";
const int mqttPort = 1883;
const char* mqttUser = "viewers";
const char* mqttPassword = "1234zxcV@";

BLEServer* pServer = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void handleRelayCommand(const String& command);

class RelayCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      handleRelayCommand(String(value.c_str()));
    }
  }
};

void connectMqtt() {
  if (!mqttClient.connected()) {
    String clientId = "BleMqttRelays-" + String(WiFi.macAddress());
    if (mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      mqttClient.subscribe("blemqtt/relays/+/set");
      mqttClient.subscribe("blemqtt/relays/+/toggle");
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  handleRelayCommand(String(topic) + ":" + message);
}

void handleRelayCommand(const String& command) {
  // Format: "blemqtt/relays/N/set:1" or "blemqtt/relays/N/toggle:"
  int indexStart = command.indexOf("relays/");
  if (indexStart < 0) return;
  int relayIndex = command.substring(indexStart + 7).toInt();
  if (relayIndex < 0 || relayIndex >= RELAY_COUNT) return;

  if (command.indexOf("/set:") >= 0) {
    bool state = command.endsWith("1");
    digitalWrite(relayPins[relayIndex], state ? HIGH : LOW);
  } else if (command.indexOf("/toggle:") >= 0) {
    digitalWrite(relayPins[relayIndex], !digitalRead(relayPins[relayIndex]));
  }

  String stateTopic = String("blemqtt/relays/") + relayIndex + "/state";
  mqttClient.publish(stateTopic.c_str(), digitalRead(relayPins[relayIndex]) ? "1" : "0", true);
}

void setupBle() {
  BLEDevice::init("BleMqttRelays");
  pServer = BLEDevice::createServer();
  BLEService* pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c00000000000");

  pCommandCharacteristic = pService->createCharacteristic(
      "beb5483e-36e1-4688-b7f5-ea07361b26a8",
      BLECharacteristic::PROPERTY_WRITE
  );
  pCommandCharacteristic->setCallbacks(new RelayCallbacks());
  pCommandCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("4fafc201-1fb5-459e-8fcc-c00000000000");
  pAdvertising->start();
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  setup_wifiManager();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
  setupBle();
}

void loop() {
  loop_wifiManager();

  if (WiFi.status() == WL_CONNECTED) {
    connectMqtt();
    mqttClient.loop();
  }
}
