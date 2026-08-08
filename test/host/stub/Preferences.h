#pragma once
#include "Arduino.h"
struct Preferences { bool begin(const char*, bool=false){return true;} void end(){}
  String getString(const char*, const char* d=""){ return String(d); }
  void putString(const char*, const String&){} };
