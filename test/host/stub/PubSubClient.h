#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"
struct PubRec { std::string topic, payload; bool retain; };
extern std::vector<PubRec> g_pubs;
extern std::vector<std::string> g_subs;
extern bool g_connected;
extern bool g_connectOk;
extern std::string g_willTopic, g_willPayload, g_clientId, g_user;
struct PubSubClient {
    typedef void (*CB)(char*, uint8_t*, unsigned int);
    CB cb = nullptr;
    PubSubClient(WiFiClientSecure&) {}
    void setCallback(CB c){ cb=c; }
    void setServer(const char*, uint16_t){}
    bool connected(){ return g_connected; }
    int state(){ return g_connectOk ? 0 : -2; }
    bool connect(const char* id, const char* u, const char*, const char* wt,
                 int, bool, const char* wp){
        g_clientId=id; g_user=u?u:""; g_willTopic=wt?wt:""; g_willPayload=wp?wp:"";
        g_connected = g_connectOk; return g_connectOk; }
    bool publish(const char* t, const char* p, bool r=false){
        g_pubs.push_back({t,p,r}); return g_connected; }
    bool subscribe(const char* t){ g_subs.push_back(t); return true; }
    void loop(){}
};
