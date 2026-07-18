#include "roleManager.h"
#include "roleConfig.h"
#include "actuatorRole.h"
#include "heaterCoolerRole.h"
#include "humidityRole.h"
#include "lightRole.h"
#include "mixedRole.h"

// Static role instances (no dynamic allocation). One is selected as active.
static HeaterCoolerRole heaterCoolerRole;
static HumidityRole     humidityRole;
static LightRole        lightRole;
static MixedRole        mixedRole;

static ActuatorRole* activeRole = nullptr;

void roleManager_setup() {
    switch (roleConfig_get().roleClass) {
        case ROLE_HEATER_COOLER: activeRole = &heaterCoolerRole; break;
        case ROLE_HUMIDITY:      activeRole = &humidityRole;     break;
        case ROLE_LIGHT:         activeRole = &lightRole;        break;
        case ROLE_MIXED:         activeRole = &mixedRole;        break;
        default:                 activeRole = nullptr;           break;  // water/unknown: manual only
    }
    if (activeRole) activeRole->setup();
    else Serial.println("No auto-control role for this function — relays are manual only.");
}

void roleManager_onConnect() {
    if (activeRole) activeRole->onConnect();
}

void roleManager_loop() {
    if (activeRole) activeRole->loop();
}

void roleManager_onSensor(const String& type, const String& field, float value) {
    if (activeRole) activeRole->onSensor(type, field, value);
}
