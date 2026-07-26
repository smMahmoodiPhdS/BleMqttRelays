#pragma once
#include <Arduino.h>

#define RELAY_COUNT 4

// ---------------------------------------------------------------------------
// Relay state, now measured rather than assumed.
//
// Before the feedback hardware existed, "relay state" meant "the last value the
// firmware wrote to the GPIO". That was a lie whenever the on-board SS-13D07
// slider was in its ALWAYS-ON or OFF position, because the slider steals the
// FET gate from the ESP entirely. This board senses the coil's low side, so we
// can now report what the relay is actually doing.
//
//   commanded = what the firmware last drove on RLn_IO (MQTT / BLE / role logic)
//   actual    = what the coil is really doing, read back from SENn
//   mode      = how those two relate (see RelayMode)
// ---------------------------------------------------------------------------

enum RelayMode : uint8_t {
    RELAY_SYNC_OK      = 0,  // actual matches commanded - the ESP is in control
    RELAY_OVERRIDE_ON  = 1,  // energized while commanded OFF  -> slider on ALWAYS-ON
    RELAY_OVERRIDE_OFF = 2,  // released while commanded ON    -> slider on OFF
    RELAY_FB_UNKNOWN   = 3,  // sense voltage stuck between thresholds
};

struct RelayStatus {
    bool      commanded;
    bool      actual;
    RelayMode mode;
    uint16_t  senseMv;   // last divider reading, for diagnostics
};

// Fires whenever a relay's *observed* state or mode changes, whoever caused it -
// an MQTT command, a BLE write, the control role, or someone's thumb on the
// slider. Transports use this to stay in sync with reality.
typedef void (*RelayStateListener)(uint8_t relayIndex, const RelayStatus& status);

void setup_relays();
void relay_loop();                       // poll the feedback inputs; call from loop()

void relay_setState(uint8_t relayIndex, bool state);   // command a relay
bool relay_getState(uint8_t relayIndex);               // observed state (what it IS)
bool relay_getCommanded(uint8_t relayIndex);           // requested state (what we ASKED)
RelayMode relay_getMode(uint8_t relayIndex);
const RelayStatus& relay_getStatus(uint8_t relayIndex);
bool relay_isOverridden(uint8_t relayIndex);           // slider (or fault) has the channel

const char* relay_modeName(RelayMode mode);

void relay_addListener(RelayStateListener listener);

// Forces a listener callback for every channel - used after a transport
// (re)connects so it can seed itself with current reality.
void relay_republishAll();
