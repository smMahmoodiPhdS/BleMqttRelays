// Host-side assertions for the two reconnect-timer fixes in BleMqttRelays.
//
// ORDER MATTERS: the reconnect state (`everAttempted`, `lastReconnectAttempt`) is
// file-static and cannot be reset from here, so the cold-boot scenario has to run
// first, on genuinely fresh statics. That scenario happens to prove both fixes at
// once, which is why it is the whole test rather than two.
#include "Arduino.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include "mqttFunctions.h"
#include "netTime.h"
#include "sensorSim.h"
#include <iostream>

extern std::vector<PubRec> g_pubs;
extern bool g_connected, g_connectOk;
extern std::string g_willTopic, g_willPayload, g_user;
extern unsigned long g_millis;
extern int g_republishCalls, g_roleOnConnect;

static int fails = 0;
static void chk(bool ok, const std::string& what, const std::string& got="") {
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what;
    if (!ok && !got.empty()) std::cout << "   [got: " << got << "]";
    std::cout << "\n"; if (!ok) fails++;
}
static const PubRec* find(const std::string& t) {
    for (auto it=g_pubs.rbegin(); it!=g_pubs.rend(); ++it) if (it->topic==t) return &*it;
    return nullptr;
}

int main() {
    setup_mqtt();
    setup_netTime();
    WiFi.st = WL_CONNECTED;
    g_connectOk = true;               // the broker would accept us if we asked

    std::cout << "-- cold boot: clock not yet synced --\n";
    g_millis = 900;
    for (int i = 0; i < 5; i++) { g_millis += 100; loop_mqtt(); }
    chk(!g_connected, "clock gate refuses while the ESP32 still believes it is 1970");

    std::cout << "\n-- NTP lands at t = 9.2 s --\n";
    netTime_loop();                   // starts SNTP
    g_millis = 5000;  netTime_loop();
    g_millis = 9200;  netTime_loop();
    chk(netTime_isSynced(), "clock is sane");

    std::cout << "\n-- both fixes, proven together --\n";
    loop_mqtt();
    chk(g_connected, "connects on the very next loop after the clock lands");
    chk(g_millis < MQTT_RECONNECT_INTERVAL_MS,
        "...and it happened at t=9.2 s, i.e. WITHOUT waiting out a 30 s interval");
    // Before the patch: the 5 clock-gate refusals armed the backoff at t=1000, so
    // this connect would have been suppressed until t~31000 - a full 22 s after the
    // clock was already good. And with lastReconnectAttempt starting at 0, even a
    // board with a good clock at boot idled until t=30000.
    chk(g_roleOnConnect == 1, "role was told about the connection");
    chk(g_republishCalls == 1, "relay states re-asserted on connect");

    std::cout << "\n-- a REAL rejection still earns the backoff --\n";
    g_connected = false; g_connectOk = false;
    g_millis = 20000; loop_mqtt();
    chk(!g_connected, "broker rejects us at t=20 s");
    g_connectOk = true;
    g_millis = 25000; loop_mqtt();
    chk(!g_connected, "retry suppressed 5 s later - backoff correctly armed");
    g_millis = 51000; loop_mqtt();
    chk(g_connected, "retry allowed once 30 s have passed");

    std::cout << "\n-- topics unchanged by this patch --\n";
    chk(g_willTopic == "actuator/smmahmoodi/farm01/rmhc1/online", "LWT topic", g_willTopic);
    chk(g_willPayload == "1", "LWT payload 1 = offline");
    const PubRec* p = find("actuator/smmahmoodi/farm01/rmhc1/online");
    chk(p && p->payload == "2" && p->retain, "retained online = 2");
    chk(find("configs/smmahmoodi/farm01/rmhc1/function") != nullptr, "function descriptor");
    chk(g_user == "dev.farm01.smmahmoodi.rmhc1", "device account", g_user);

    std::cout << "\n-- sensorSim no longer writes daily_* (contract 3.2) --\n";
    g_pubs.clear();
    for (int i = 0; i < 40; i++) { g_millis += 6000; sensorSim_loop(); }
    bool anyDaily = false, anyCu = false, anyRetainedCu = false;
    for (auto& r : g_pubs) {
        if (r.topic.find("daily_") != std::string::npos) anyDaily = true;
        if (r.topic.find("/cu") != std::string::npos) {
            anyCu = true;
            if (r.retain) anyRetainedCu = true;
        }
    }
    chk(anyCu, "still publishes cu");
    chk(!anyDaily, "publishes NO daily_max / daily_min - Node-RED owns those");
    chk(!anyRetainedCu, "no cu publish is retained");

    std::cout << "\n" << (fails ? "FAILURES: " : "ALL PASS (") << fails
              << (fails ? "" : " failures)") << "\n";
    return fails ? 1 : 0;
}
