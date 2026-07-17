#include "roleConfig.h"
#include "boardConfig.h"

// Locked starter function table (Addressing doc §2.3). Index by sw2 value.
// Humidity (8..11) and water (12..15) maps are still TBD — reserved here.
static const RoleConfig FUNCTION_TABLE[] = {
    {0, ROLE_HEATER_COOLER, "FN_ICH_H1234",  "rmhc", {RF_HEATER, RF_HEATER, RF_HEATER, RF_HEATER}},
    {1, ROLE_HEATER_COOLER, "FN_ICH_C1234",  "rmhc", {RF_COOLER, RF_COOLER, RF_COOLER, RF_COOLER}},
    {2, ROLE_HEATER_COOLER, "FN_ICH_H12C34", "rmhc", {RF_HEATER, RF_HEATER, RF_COOLER, RF_COOLER}},
    {3, ROLE_HEATER_COOLER, "FN_ICH_H34C12", "rmhc", {RF_COOLER, RF_COOLER, RF_HEATER, RF_HEATER}},
    {4, ROLE_LIGHT,         "FN_ICL_SSSS",   "rmlt", {RF_LIGHT_SALOON, RF_LIGHT_SALOON, RF_LIGHT_SALOON, RF_LIGHT_SALOON}},
    {5, ROLE_LIGHT,         "FN_ICL_SSPM",   "rmlt", {RF_LIGHT_SALOON, RF_LIGHT_SALOON, RF_LIGHT_PERIPHERAL, RF_LIGHT_MANAGER}},
};
static const size_t FUNCTION_TABLE_LEN = sizeof(FUNCTION_TABLE) / sizeof(FUNCTION_TABLE[0]);

// Fallback for reserved / unrecognized function-switch values.
static const RoleConfig UNKNOWN_ROLE = {
    0xFF, ROLE_UNKNOWN, "FN_UNKNOWN", "rmun", {RF_UNUSED, RF_UNUSED, RF_UNUSED, RF_UNUSED}
};

static RoleConfig currentRole = UNKNOWN_ROLE;

void roleConfig_setup() {
    currentRole = UNKNOWN_ROLE;
    for (size_t i = 0; i < FUNCTION_TABLE_LEN; i++) {
        if (FUNCTION_TABLE[i].functionValue == functionValue) {
            currentRole = FUNCTION_TABLE[i];
            break;
        }
    }
    Serial.print("Role: sw2=");
    Serial.print(functionValue);
    Serial.print(" -> ");
    Serial.print(currentRole.fnName);
    Serial.print("  prefix=");
    Serial.print(currentRole.topicPrefix);
    Serial.print(roleConfig_address());
    Serial.println();
}

const RoleConfig& roleConfig_get() {
    return currentRole;
}

uint8_t roleConfig_address() {
    return addressValue + 1;  // 1-based topic index
}
