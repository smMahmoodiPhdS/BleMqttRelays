#pragma once
#define CFG_NAME_LEN 65
#define CFG_HOST_LEN 96
#define CFG_USER_LEN 96
#define CFG_PASS_LEN 65
extern char farmOwner[CFG_NAME_LEN];
extern char farmId[CFG_NAME_LEN];
extern char mqttHost[CFG_HOST_LEN];
extern char mqttPort[6];
extern char mqttUser[CFG_USER_LEN];
extern char mqttPassword[CFG_PASS_LEN];
void setup_wifiManager();
void loop_wifiManager();
bool config_isComplete();
const char* config_problem();
bool config_nameIsLegal(const char*);
