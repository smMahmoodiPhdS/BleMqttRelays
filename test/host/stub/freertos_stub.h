#pragma once
#define pdMS_TO_TICKS(x) (x)
#define APP_CPU_NUM 1
inline void vTaskDelay(unsigned){}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void*,int){return 1;}
