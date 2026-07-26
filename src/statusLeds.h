#pragma once
#include <Arduino.h>

// The board has two discrete indicator LEDs that the firmware never drove:
//   LED_STATUS (GPIO2)  - GPIO -> 1k -> LED -> GND, active HIGH
//   LED_BLE    (GPIO12) - same topology
// Both are strapping pins, but the load is to ground so neither is pulled high
// at reset: GPIO2 must be low to boot from flash and GPIO12 (MTDI) must be low
// to keep the 3.3 V flash voltage. Driving them only after boot is safe.
//
// Meaning:
//   LED_STATUS  solid  = MQTT connected
//               blink  = WiFi up, broker unreachable
//               off    = no WiFi
//   LED_BLE     solid  = a BLE central is connected
//               blink  = advertising
// Any relay in an override state overrides LED_STATUS with a fast double-blink,
// so a walk past the panel shows that a slider is not in AUTO.

void setup_statusLeds();
void statusLeds_loop();

void statusLeds_setBleConnected(bool connected);
