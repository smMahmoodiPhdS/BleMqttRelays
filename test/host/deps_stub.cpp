// Minimal stand-ins for the modules mqttFunctions talks to. Enough to link and to
// drive the reconnect logic; not a simulation of relay behaviour.
#include "Arduino.h"
#include "relayManager.h"
#include "roleConfig.h"
#include "roleManager.h"
#include "sensorSim.h"

static RelayStatus st[RELAY_COUNT] = {};
static RoleConfig rc = { 2, ROLE_HEATER_COOLER, "FN_HTC_H12C34", "rmhc",
                         "heater/cooler", { RF_HEATER, RF_HEATER, RF_COOLER, RF_COOLER } };

void setup_relays() {}
void relay_loop() {}
void relay_setState(uint8_t, bool) {}
bool relay_getState(uint8_t) { return false; }
bool relay_getCommanded(uint8_t) { return false; }
RelayMode relay_getMode(uint8_t) { return RELAY_SYNC_OK; }
const RelayStatus& relay_getStatus(uint8_t i) { return st[i]; }
bool relay_isOverridden(uint8_t) { return false; }
const char* relay_modeName(RelayMode) { return "auto"; }
void relay_addListener(RelayStateListener) {}
int g_republishCalls = 0;
void relay_republishAll() { g_republishCalls++; }

void roleConfig_setup() {}
const RoleConfig& roleConfig_get() { return rc; }
uint8_t roleConfig_address() { return 1; }
bool roleUsesTemperature() { return true; }
bool roleUsesHumidity() { return false; }
bool roleUsesLight() { return false; }

int g_roleOnConnect = 0;
void roleManager_setup() {}
void roleManager_onConnect() { g_roleOnConnect++; }
void roleManager_loop() {}
void roleManager_onSensor(const String&, const String&, float) {}

