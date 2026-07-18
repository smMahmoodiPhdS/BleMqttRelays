#include "mixedRole.h"
#include "roleConfig.h"
#include "controlParams.h"
#include "mqttFunctions.h"

void MixedRole::setup() {
    const RoleConfig& r = roleConfig_get();
    for (uint8_t i = 0; i < 4; i++) {
        Slot& s = _slots[i];
        switch (r.relayFunc[i]) {
            case RF_HEATER:
                s.active = true; s.sensorType = "ts"; s.statusField = "hOn";
                s.ctrl.configure(DIR_RAISE, TEMP_HYSTERESIS_C, CTRL_MIN_ON_MS, CTRL_MIN_OFF_MS);
                s.ctrl.addRelay(i);
                break;
            case RF_COOLER:
                s.active = true; s.sensorType = "ts"; s.statusField = "cOn";
                s.ctrl.configure(DIR_LOWER, TEMP_HYSTERESIS_C, CTRL_MIN_ON_MS, CTRL_MIN_OFF_MS);
                s.ctrl.addRelay(i);
                break;
            case RF_HUMIDIFIER:
                s.active = true; s.sensorType = "hs"; s.statusField = "hOn";
                s.ctrl.configure(DIR_RAISE, HUM_HYSTERESIS_PCT, CTRL_MIN_ON_MS, CTRL_MIN_OFF_MS);
                s.ctrl.addRelay(i);
                break;
            default:
                s.active = false;  // light / unused -> manual
                break;
        }
    }
    Serial.println("Role Mixed (versatile) ready — per-relay control.");
}

void MixedRole::onSensor(const String& type, const String& field, float value) {
    for (uint8_t i = 0; i < 4; i++) {
        Slot& s = _slots[i];
        if (!s.active || type != s.sensorType) continue;
        if (field == "cu") s.ctrl.setCurrent(value);
        else if (field == "ll") s.ctrl.setLower(value);
        else if (field == "ul") s.ctrl.setUpper(value);
    }
}

void MixedRole::loop() {
    unsigned long now = millis();
    if (now - _lastEval < CTRL_INTERVAL_MS) return;
    _lastEval = now;

    for (uint8_t i = 0; i < 4; i++) {
        Slot& s = _slots[i];
        if (!s.active) continue;
        if (s.ctrl.update()) {
            Serial.print("[CTRL] rmmx rl0");
            Serial.print(i + 1);
            Serial.print(" (");
            Serial.print(s.sensorType);
            Serial.print("/");
            Serial.print(s.statusField);
            Serial.print(") -> ");
            Serial.println(s.ctrl.isOn() ? "ON" : "OFF");
            publishSlot(i);
        }
    }
}

void MixedRole::onConnect() {
    for (uint8_t i = 0; i < 4; i++) if (_slots[i].active) publishSlot(i);
}

void MixedRole::publishSlot(uint8_t i) {
    Slot& s = _slots[i];
    mqtt_publishSensorStatus(s.sensorType, s.statusField, s.ctrl.isOn());
}
