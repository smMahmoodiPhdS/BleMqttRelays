#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"
enum t_httpUpdate_return { HTTP_UPDATE_FAILED, HTTP_UPDATE_NO_UPDATES, HTTP_UPDATE_OK };
struct HttpUpdateCls {
  void onStart(void(*)()){} void onEnd(void(*)()){} 
  void onProgress(void(*)(int,int)){} void onError(void(*)(int)){}
  int getLastError(){return 0;} String getLastErrorString(){return String("");}
  t_httpUpdate_return update(WiFiClientSecure&, const String&){ return HTTP_UPDATE_OK; } };
extern HttpUpdateCls httpUpdate;
