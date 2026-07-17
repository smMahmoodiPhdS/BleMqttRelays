#pragma once
#include <Arduino.h>

// Decodes the function switch (sw2 / functionValue) into the board's role at
// runtime — one binary, no #ifdef. See Docs/Architecture/Addressing-and-Topic-Design.md.

enum RoleClass {
    ROLE_HEATER_COOLER,
    ROLE_LIGHT,
    ROLE_HUMIDITY,
    ROLE_WATER,
    ROLE_UNKNOWN
};

// Per-relay assignment. Heater/cooler roles use HEATER/COOLER; light roles use
// the LIGHT_* zones (informational for now — light scheduling is TODO).
enum RelayFunc {
    RF_HEATER,
    RF_COOLER,
    RF_LIGHT_SALOON,
    RF_LIGHT_PERIPHERAL,
    RF_LIGHT_MANAGER,
    RF_UNUSED
};

struct RoleConfig {
    uint8_t     functionValue;   // raw sw2 nibble (0..15)
    RoleClass   roleClass;
    const char* fnName;          // e.g. "FN_ICH_H12C34" (published as descriptor)
    const char* topicPrefix;     // e.g. "rmhc"
    RelayFunc   relayFunc[4];    // per-relay role
};

void roleConfig_setup();             // decode functionValue -> role
const RoleConfig& roleConfig_get();  // current role
uint8_t roleConfig_address();        // 1-based module address (addressValue + 1)
