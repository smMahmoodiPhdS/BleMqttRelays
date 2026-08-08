#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
struct WiFiCls { int st = WL_CONNECTED; int status(){ return st; } };
extern WiFiCls WiFi;
