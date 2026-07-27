/*
    ESP32 firmware for a 4-relay controller, reachable over BLE and MQTT,
    with WiFi provisioning via a captive portal (IotWebConf).

    Board: Hardware/kicad/actuator-node-Ble (see boardConfig.h for the pin map).

    Identity and broker settings come from the captive portal; nothing is
    compiled in. Topics use the Option A namespace - the farm has its own level:

      actuator/<owner>/<farm>/<module>/rl0N/on     -> command relay N on
      actuator/<owner>/<farm>/<module>/rl0N/off    -> command relay N off
      actuator/<owner>/<farm>/<module>/rl0N/state  -> MEASURED coil state
                                                      (retained) 1 = off, 2 = on
      actuator/<owner>/<farm>/<module>/rl0N/cmd    -> last commanded state
      actuator/<owner>/<farm>/<module>/rl0N/mode   -> auto | override_on |
                                                      override_off | unknown

    Transport is MQTT over TLS on 8883, validating against a pinned ISRG Root
    X1. There is no plaintext fallback and no old-broker path.

    `state` comes from the board's drain-sense feedback, so it is correct even
    when the on-board SS-13D07 slider - not the ESP - is driving the coil. When
    `mode` is not "auto" the slider (or a hardware fault) owns that channel and
    commands will not change anything.

    BLE: one service; one read/write/notify characteristic per relay (0x01 = on),
    plus a packed 4-byte status characteristic carrying actual + commanded + mode
    for all channels. Service/characteristic UUIDs are derived from farmOwner and
    the DIP switch value so multiple boards don't collide.
*/

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_random.h>
#include "boardConfig.h"
#include "relayManager.h"
#include "wifiManagerTools.h"
#include "mqttFunctions.h"
#include "bleRelayServer.h"
#include "otaUpdater.h"
#include "roleConfig.h"
#include "roleManager.h"
#include "hwWatchdog.h"
#include "statusLeds.h"
#include "netTime.h"

#define WATCHDOG_TIMEOUT_S 30

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n------------ BleMqttRelays booting -------------\n");

    // Kick the external TLC555 first: it has been counting since power-on and
    // its timeout is only ~10 s.
    setup_hwWatchdog();

    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    setup_statusLeds();

    boardConfig_readDipSwitches();

    // Seed from the hardware RNG. The old analogRead(36) seed would now fight
    // the relay-1 feedback divider - GPIO36 is SENSOR_VP / SEN3 on this board.
    randomSeed(esp_random());

    Serial.print("DIP raw value: ");
    Serial.print(dipValue);
    Serial.print(" = b");   // 8-bit binary, function nibble _ address nibble
    for (int b = 7; b >= 0; b--) {
        Serial.print((dipValue >> b) & 1);
        if (b == 4) Serial.print('_');
    }
    Serial.println();

    // Decode the function switch into the board's role (prints address + function
    // with human-readable descriptions, wrapped in dashed separators).
    roleConfig_setup();

    setup_relays();
    setup_wifiManager();
    setup_netTime();     // must precede any TLS attempt - see netTime.h
    setup_mqtt();
    setup_bleRelayServer();
    setup_ota();
    roleManager_setup();   // selects + sets up the role from the function switch

    Serial.println("Setup complete.");
}

void loop() {
    esp_task_wdt_reset();
    relay_loop();          // poll coil feedback; fans out to MQTT + BLE on change
    loop_wifiManager();
    netTime_loop();        // gates loop_mqtt(): no sane clock, no TLS
    loop_mqtt();
    loop_ota();
    roleManager_loop();
    statusLeds_loop();
}
