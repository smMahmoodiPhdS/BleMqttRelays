#pragma once
#include <Arduino.h>

// Decodes the function switch (sw2 / functionValue) into the board's role at
// runtime — one binary, no #ifdef. See Docs/Architecture/APP-AND-CONTRACT.md §1.3.

enum RoleClass {
    ROLE_HEATER_COOLER,
    ROLE_LIGHT,
    ROLE_HUMIDITY,
    ROLE_WATER,
    ROLE_MIXED,
    ROLE_UNKNOWN
};

// Per-relay assignment. A relay is one of these independent actuator functions.
enum RelayFunc {
    RF_HEATER,            // ON when temperature below lower limit
    RF_COOLER,            // ON when temperature above upper limit
    RF_HUMIDIFIER,        // ON when humidity below lower limit
    RF_LIGHT_SALOON,      // light zone (scheduling TODO — manual for now)
    RF_LIGHT_PERIPHERAL,
    RF_LIGHT_MANAGER,
    RF_UNUSED
};

struct RoleConfig {
    uint8_t     functionValue;   // raw sw2 nibble (0..15)
    RoleClass   roleClass;
    const char* fnName;          // e.g. "FN_HTC_H12C34" (published as descriptor)
    const char* topicPrefix;     // e.g. "rmhc"
    const char* desc;            // short human-readable description for the log
    RelayFunc   relayFunc[4];    // per-relay role
};

void roleConfig_setup();             // decode functionValue -> role
const RoleConfig& roleConfig_get();  // current role
uint8_t roleConfig_address();        // 1-based module address (addressValue + 1)

// Which sensor streams does the current role consume (derived from relayFunc)?
bool roleUsesTemperature();          // any RF_HEATER / RF_COOLER
bool roleUsesHumidity();             // any RF_HUMIDIFIER
bool roleUsesLight();                // any RF_LIGHT_*
