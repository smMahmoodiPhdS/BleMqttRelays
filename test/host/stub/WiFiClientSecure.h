#pragma once
#include "Arduino.h"
struct WiFiClientSecure { const char* ca=nullptr; void setCACert(const char* c){ ca=c; } };
