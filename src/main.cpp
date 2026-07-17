/*
    ESP32 firmware for a 4-relay controller, reachable over BLE and MQTT,
    with WiFi provisioning via a captive portal (IotWebConf).

    Relay MQTT topics (farmOwner is set via the captive portal):
      actuator/<farmOwner>/rl0N/on      -> turn relay N on
      actuator/<farmOwner>/rl0N/off     -> turn relay N off
      actuator/<farmOwner>/rl0N/state   -> published (retained) on every change,
                                           value 1 = off, 2 = on

    BLE: one service, one read/write characteristic per relay. Writing 0x01
    turns the relay ON, any other byte turns it OFF. Service/characteristic
    UUIDs are derived from farmOwner + the DIP switch value so multiple
    boards don't collide.
*/

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "boardConfig.h"
#include "relayManager.h"
#include "wifiManagerTools.h"
#include "mqttFunctions.h"
#include "bleRelayServer.h"
#include "otaUpdater.h"

#define WATCHDOG_TIMEOUT_S 30

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n------------ BleMqttRelays booting -------------\n");

    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    boardConfig_readDipSwitches();
    randomSeed(analogRead(boardPins.randomSeedPin));
    Serial.print("DIP value: ");
    Serial.println(dipValue);

    setup_relays();
    setup_wifiManager();
    setup_mqtt();
    setup_bleRelayServer();
    setup_ota();

    Serial.println("Setup complete.");
}

void loop() {
    esp_task_wdt_reset();
    loop_wifiManager();
    loop_mqtt();
    loop_ota();
}
