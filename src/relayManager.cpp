#include "relayManager.h"
#include "boardConfig.h"
#include <Preferences.h>

#define MAX_RELAY_LISTENERS 4

static bool relayState[RELAY_COUNT];
static Preferences relayPrefs;
static RelayStateListener listeners[MAX_RELAY_LISTENERS];
static uint8_t listenerCount = 0;

static void notifyListeners(uint8_t relayIndex, bool state) {
    for (uint8_t i = 0; i < listenerCount; i++) {
        listeners[i](relayIndex, state);
    }
}

void relay_addListener(RelayStateListener listener) {
    if (listenerCount < MAX_RELAY_LISTENERS) {
        listeners[listenerCount++] = listener;
    }
}

void setup_relays() {
    relayPrefs.begin("relays", false);
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        Serial.print("[TMP Relay Debug] Initializing Relay ");
        Serial.print(i + 1);
        Serial.print(" on GPIO ");
        Serial.println(boardPins.relayPin[i]);

        pinMode(boardPins.relayPin[i], OUTPUT);
        bool restored = relayPrefs.getBool(String("r" + String(i)).c_str(), false);
        relayState[i] = restored;
        digitalWrite(boardPins.relayPin[i], restored ? HIGH : LOW);
    }
    relayPrefs.end();
}

void relay_setState(uint8_t relayIndex, bool state) {
    if (relayIndex >= RELAY_COUNT) return;

    Serial.print("[TMP Relay Debug] Setting Relay ");
    Serial.print(relayIndex + 1);
    Serial.print(" to ");
    Serial.println(state ? "ON" : "OFF");

    relayState[relayIndex] = state;
    digitalWrite(boardPins.relayPin[relayIndex], state ? HIGH : LOW);

    relayPrefs.begin("relays", false);
    relayPrefs.putBool(String("r" + String(relayIndex)).c_str(), state);
    relayPrefs.end();

    notifyListeners(relayIndex, state);
}

void relay_toggle(uint8_t relayIndex) {
    if (relayIndex >= RELAY_COUNT) return;
    relay_setState(relayIndex, !relayState[relayIndex]);
}

bool relay_getState(uint8_t relayIndex) {
    if (relayIndex >= RELAY_COUNT) return false;
    return relayState[relayIndex];
}
