#include "humidityRole.h"
#include "roleConfig.h"
#include "controlParams.h"
#include "mqttFunctions.h"

void HumidityRole::setup() {
    _humidifier.configure(DIR_RAISE, HUM_HYSTERESIS_PCT, CTRL_MIN_ON_MS, CTRL_MIN_OFF_MS);
    const RoleConfig& r = roleConfig_get();
    for (uint8_t i = 0; i < 4; i++) {
        if (r.relayFunc[i] == RF_HUMIDIFIER) _humidifier.addRelay(i);
    }
    Serial.print("Role Humidity ready, follows hs");
    Serial.println(roleConfig_address());
}

void HumidityRole::onSensor(const String& type, const String& field, float value) {
    if (type != "hs") return;
    if (field == "cu") _humidifier.setCurrent(value);
    else if (field == "ll") _humidifier.setLower(value);
    else if (field == "ul") _humidifier.setUpper(value);
}

void HumidityRole::loop() {
    unsigned long now = millis();
    if (now - _lastEval < CTRL_INTERVAL_MS) return;
    _lastEval = now;

    if (_humidifier.update()) {
        Serial.print("[CTRL] hs");
        Serial.print(roleConfig_address());
        Serial.print(" -> Humidifier ");
        Serial.println(_humidifier.isOn() ? "ON" : "OFF");
        publishStatus();
    }
}

void HumidityRole::onConnect() {
    publishStatus();
}

void HumidityRole::publishStatus() {
    // hs<A>/hOn = humidifier status (the humidity card already reads it).
    mqtt_publishSensorStatus("hs", "hOn", _humidifier.isOn());
}
