#include "roleConfig.h"
#include "boardConfig.h"

// Locked function table (Addressing doc §2.3). Index by sw2 value.
//   0-3  heater/cooler (rmhc)   4-5 light (rmlt)   8 humidity (rmhu)
//   12   versatile/mixed (rmmx)   others reserved.
static const RoleConfig FUNCTION_TABLE[] = {
    {0,  ROLE_HEATER_COOLER, "FN_HTC_H1234",  "rmhc", "Heater/Cooler: all 4 relays HEAT",        {RF_HEATER, RF_HEATER, RF_HEATER, RF_HEATER}},
    {1,  ROLE_HEATER_COOLER, "FN_HTC_C1234",  "rmhc", "Heater/Cooler: all 4 relays COOL",        {RF_COOLER, RF_COOLER, RF_COOLER, RF_COOLER}},
    {2,  ROLE_HEATER_COOLER, "FN_HTC_H12C34", "rmhc", "Heater/Cooler: rl1,2 HEAT  rl3,4 COOL",   {RF_HEATER, RF_HEATER, RF_COOLER, RF_COOLER}},
    {3,  ROLE_HEATER_COOLER, "FN_HTC_H34C12", "rmhc", "Heater/Cooler: rl3,4 HEAT  rl1,2 COOL",   {RF_COOLER, RF_COOLER, RF_HEATER, RF_HEATER}},
    {4,  ROLE_LIGHT,         "FN_LGT_SSSS",   "rmlt", "Light: all 4 relays SALOON",              {RF_LIGHT_SALOON, RF_LIGHT_SALOON, RF_LIGHT_SALOON, RF_LIGHT_SALOON}},
    {5,  ROLE_LIGHT,         "FN_LGT_SSPM",   "rmlt", "Light: saloon,saloon,peripheral,manager", {RF_LIGHT_SALOON, RF_LIGHT_SALOON, RF_LIGHT_PERIPHERAL, RF_LIGHT_MANAGER}},
    {8,  ROLE_HUMIDITY,      "FN_HUM_SSSS",   "rmhu", "Humidity: all 4 relays HUMIDIFY",         {RF_HUMIDIFIER, RF_HUMIDIFIER, RF_HUMIDIFIER, RF_HUMIDIFIER}},
    {12, ROLE_MIXED,         "FN_MIX_HuLgCoHt","rmmx","Versatile: rl1 Humid, rl2 Light, rl3 Cool, rl4 Heat", {RF_HUMIDIFIER, RF_LIGHT_SALOON, RF_COOLER, RF_HEATER}},
};
static const size_t FUNCTION_TABLE_LEN = sizeof(FUNCTION_TABLE) / sizeof(FUNCTION_TABLE[0]);

// Fallback for reserved / unrecognized function-switch values.
static const RoleConfig UNKNOWN_ROLE = {
    0xFF, ROLE_UNKNOWN, "FN_UNKNOWN", "rmun", "Unknown/reserved function switch",
    {RF_UNUSED, RF_UNUSED, RF_UNUSED, RF_UNUSED}
};

static RoleConfig currentRole = UNKNOWN_ROLE;

// Print a 4-bit value as zero-padded binary, e.g. 2 -> "0010".
static void printNibbleBin(uint8_t v) {
    for (int b = 3; b >= 0; b--) Serial.print((v >> b) & 1);
}

void roleConfig_setup() {
    currentRole = UNKNOWN_ROLE;
    for (size_t i = 0; i < FUNCTION_TABLE_LEN; i++) {
        if (FUNCTION_TABLE[i].functionValue == functionValue) {
            currentRole = FUNCTION_TABLE[i];
            break;
        }
    }

    Serial.println("-------------------------------");
    // Address readout (sw1): decimal + 4-bit binary.
    Serial.print("Address  (sw1=");
    Serial.print(addressValue);
    Serial.print(", b");
    printNibbleBin(addressValue);
    Serial.print(") -> module #");
    Serial.print(roleConfig_address());
    Serial.print("  topic: ");
    Serial.print(currentRole.topicPrefix);
    Serial.println(roleConfig_address());
    // Function readout (sw2): decimal + 4-bit binary.
    Serial.print("Function (sw2=");
    Serial.print(functionValue);
    Serial.print(", b");
    printNibbleBin(functionValue);
    Serial.print(") -> ");
    Serial.print(currentRole.fnName);
    Serial.print("  | ");
    Serial.println(currentRole.desc);
    Serial.println("-------------------------------");
}

const RoleConfig& roleConfig_get() {
    return currentRole;
}

uint8_t roleConfig_address() {
    return addressValue + 1;  // 1-based topic index
}

bool roleUsesTemperature() {
    for (uint8_t i = 0; i < 4; i++) {
        RelayFunc f = currentRole.relayFunc[i];
        if (f == RF_HEATER || f == RF_COOLER) return true;
    }
    return false;
}

bool roleUsesHumidity() {
    for (uint8_t i = 0; i < 4; i++) {
        if (currentRole.relayFunc[i] == RF_HUMIDIFIER) return true;
    }
    return false;
}

bool roleUsesLight() {
    for (uint8_t i = 0; i < 4; i++) {
        RelayFunc f = currentRole.relayFunc[i];
        if (f == RF_LIGHT_SALOON || f == RF_LIGHT_PERIPHERAL || f == RF_LIGHT_MANAGER) return true;
    }
    return false;
}
