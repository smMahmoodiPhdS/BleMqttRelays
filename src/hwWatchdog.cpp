#include "hwWatchdog.h"
#include "boardConfig.h"

// Pin 0 means the board has no external watchdog wired.
static inline bool hasWatchdog() { return boardPins.watchdogHeartbeat != 0; }

void hwWatchdog_kick() {
    if (!hasWatchdog()) return;
    digitalWrite(boardPins.watchdogHeartbeat, LOW);   // discharge C_WD_T
    delay(HW_WATCHDOG_PULSE_MS);
    digitalWrite(boardPins.watchdogHeartbeat, HIGH);  // release, timer restarts
}

static void hwWatchdogTask(void* arg) {
    (void)arg;
    for (;;) {
        hwWatchdog_kick();
        vTaskDelay(pdMS_TO_TICKS(HW_WATCHDOG_KICK_INTERVAL_MS - HW_WATCHDOG_PULSE_MS));
    }
}

void setup_hwWatchdog() {
    if (!hasWatchdog()) {
        Serial.println("[watchdog] no external TLC555 on this board — "
                       "relying on the ESP32 task watchdog only");
        return;
    }
    pinMode(boardPins.watchdogHeartbeat, OUTPUT);
    digitalWrite(boardPins.watchdogHeartbeat, HIGH);
    hwWatchdog_kick();   // start from a known-discharged timing cap

    // Low priority: this must never starve the radio stacks, and a 2 s period
    // against a 10 s timeout leaves plenty of slack if it is briefly delayed.
    xTaskCreatePinnedToCore(hwWatchdogTask, "hwWatchdog", 2048, nullptr, 1, nullptr, APP_CPU_NUM);
    Serial.printf("[watchdog] TLC555 heartbeat on GPIO%u, %u ms period (~10 s timeout)\n",
                  boardPins.watchdogHeartbeat, HW_WATCHDOG_KICK_INTERVAL_MS);
}
