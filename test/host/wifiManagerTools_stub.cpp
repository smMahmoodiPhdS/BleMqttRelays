// Stands in for wifiManagerTools.cpp (which needs IotWebConf). That file is
// copied verbatim from the known-good actuator build; only a <title> changed.
#include "Arduino.h"
#include <cstring>
#define CFG_NAME_LEN 65
#define CFG_HOST_LEN 96
#define CFG_USER_LEN 96
#define CFG_PASS_LEN 65
char farmOwner[CFG_NAME_LEN]    = "smmahmoodi";
char farmId[CFG_NAME_LEN]       = "farm01";
char mqttHost[CFG_HOST_LEN]     = "link.asanautomation.com";
char mqttPort[6]                = "8883";
char mqttUser[CFG_USER_LEN]     = "dev.farm01.smmahmoodi.rmhc1";
char mqttPassword[CFG_PASS_LEN] = "s3cret";
static const char* problem = nullptr;
bool config_isComplete(){ return problem == nullptr; }
const char* config_problem(){ return problem; }
void setup_wifiManager(){}
void loop_wifiManager(){}
