#include "lightRole.h"
#include "roleConfig.h"

void LightRole::setup() {
    Serial.print("Role Light ready (relays manual; scheduling TODO), sensor ls");
    Serial.println(roleConfig_address());
}
