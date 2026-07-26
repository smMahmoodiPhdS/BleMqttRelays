#include "statusLeds.h"
#include "boardConfig.h"
#include "relayManager.h"
#include "mqttFunctions.h"
#include <WiFi.h>

static bool bleConnected = false;
static unsigned long lastTick = 0;
static uint8_t phase = 0;   // 0..7, 125 ms per step -> 1 s cycle

// A pin of 0 means "this board has no such LED" (GPIO0 is the BOOT strap and is
// never an indicator). The protoboard variant has neither.
static inline bool hasPin(uint8_t p) { return p != 0; }

void setup_statusLeds() {
    if (hasPin(boardPins.ledStatus)) {
        pinMode(boardPins.ledStatus, OUTPUT);
        digitalWrite(boardPins.ledStatus, LOW);
    }
    if (hasPin(boardPins.ledBle)) {
        pinMode(boardPins.ledBle, OUTPUT);
        digitalWrite(boardPins.ledBle, LOW);
    }
}

void statusLeds_setBleConnected(bool connected) {
    bleConnected = connected;
}

static bool anyRelayOverridden() {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        if (relay_isOverridden(i)) return true;
    }
    return false;
}

void statusLeds_loop() {
    unsigned long now = millis();
    if (now - lastTick < 125) return;
    lastTick = now;
    phase = (phase + 1) & 0x07;

    bool status;
    if (anyRelayOverridden()) {
        status = (phase == 0 || phase == 2);          // fast double-blink
    } else if (mqttClient.connected()) {
        status = true;                                 // solid
    } else if (WiFi.status() == WL_CONNECTED) {
        status = (phase < 4);                           // slow blink
    } else {
        status = false;
    }
    if (hasPin(boardPins.ledStatus)) {
        digitalWrite(boardPins.ledStatus, status ? HIGH : LOW);
    }
    if (hasPin(boardPins.ledBle)) {
        digitalWrite(boardPins.ledBle, bleConnected ? HIGH : (phase < 1 ? HIGH : LOW));
    }
}
