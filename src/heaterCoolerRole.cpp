#include "heaterCoolerRole.h"
#include "roleConfig.h"
#include "controlParams.h"
#include "mqttFunctions.h"

void HeaterCoolerRole::setup() {
    _heater.configure(DIR_RAISE, TEMP_HYSTERESIS_C, CTRL_MIN_ON_MS, CTRL_MIN_OFF_MS);
    _cooler.configure(DIR_LOWER, TEMP_HYSTERESIS_C, CTRL_MIN_ON_MS, CTRL_MIN_OFF_MS);
    const RoleConfig& r = roleConfig_get();
    for (uint8_t i = 0; i < 4; i++) {
        if (r.relayFunc[i] == RF_HEATER) _heater.addRelay(i);
        else if (r.relayFunc[i] == RF_COOLER) _cooler.addRelay(i);
    }
    Serial.print("Role HeaterCooler ready, follows ts");
    Serial.println(roleConfig_address());
}

void HeaterCoolerRole::onSensor(const String& type, const String& field, float value) {
    if (type != "ts") return;
    if (field == "cu") { _heater.setCurrent(value); _cooler.setCurrent(value); }
    else if (field == "ll") { _heater.setLower(value); _cooler.setLower(value); }
    else if (field == "ul") { _heater.setUpper(value); _cooler.setUpper(value); }
}

void HeaterCoolerRole::loop() {
    unsigned long now = millis();
    if (now - _lastEval < CTRL_INTERVAL_MS) return;
    _lastEval = now;

    bool changed = false;
    if (_heater.update()) changed = true;
    if (_cooler.update()) changed = true;
    if (changed) {
        Serial.print("[CTRL] ts");
        Serial.print(roleConfig_address());
        Serial.print(" -> Heater ");
        Serial.print(_heater.isOn() ? "ON" : "OFF");
        Serial.print(", Cooler ");
        Serial.print(_cooler.isOn() ? "ON" : "OFF");
        if (_heater.isOverridden() || _cooler.isOverridden()) {
            Serial.print("  (WARNING: a channel is not following commands)");
        }
        Serial.println();
    }

    // publishStatus() is self-guarding, so this also catches a coil state that
    // changed under us via the slider without any control decision.
    publishStatus();
}

void HeaterCoolerRole::onConnect() {
    _havePublished = false;   // force a publish onto a freshly connected broker
    publishStatus();
}

// hOn/cOn now report what the actuator is *doing*, not what the controller
// decided, because the coil-sense feedback lets us know the difference. A
// slider parked on ALWAYS-ON shows up here as a heater that is on while the
// thermostat wants it off - which is exactly the condition an operator wants
// to see on the dashboard.
void HeaterCoolerRole::publishStatus() {
    bool h = _heater.isActuallyOn();
    bool c = _cooler.isActuallyOn();
    if (_havePublished && h == _lastPubHeater && c == _lastPubCooler) return;

    mqtt_publishSensorStatus("ts", "hOn", h);
    mqtt_publishSensorStatus("ts", "cOn", c);
    _lastPubHeater = h;
    _lastPubCooler = c;
    _havePublished = true;
}
