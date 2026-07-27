#include "netTime.h"
#include <WiFi.h>
#include <time.h>

// Three servers: the global pool, the Iran pool, and Cloudflare as a fixed-IP
// fallback. The regional pool is usually fastest; the fixed host matters when
// pool DNS is unreliable, which is exactly the situation where a board would
// otherwise sit at 1970 forever.
static const char* NTP_1 = "pool.ntp.org";
static const char* NTP_2 = "ir.pool.ntp.org";
static const char* NTP_3 = "time.cloudflare.com";

static bool synced = false;
static bool configured = false;
static unsigned long lastAttempt = 0;
static uint8_t attempts = 0;

// Backoff: quick at first, then patient. A board that boots before its router
// has a WAN link should not hammer NTP for hours.
static unsigned long backoffMs() {
    if (attempts < 5)  return 3000UL;
    if (attempts < 15) return 15000UL;
    return 60000UL;
}

static bool clockIsSane() {
    return time(nullptr) >= (time_t)NTP_SANE_EPOCH;
}

void setup_netTime() {
    synced = false;
    configured = false;
    attempts = 0;
    lastAttempt = 0;
    Serial.println("[time] waiting for NTP before any TLS attempt");
}

void netTime_loop() {
    if (synced) return;
    if (WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();
    if (lastAttempt != 0 && (now - lastAttempt) < backoffMs()) return;
    lastAttempt = now;

    if (!configured) {
        // UTC throughout. Local time is a display concern and belongs in the
        // app or Grafana; carrying a timezone here would only add a way for
        // timestamps to disagree between devices.
        configTime(0, 0, NTP_1, NTP_2, NTP_3);
        configured = true;
        Serial.println("[time] SNTP started");
        return;   // give it one backoff interval before judging
    }

    attempts++;
    if (clockIsSane()) {
        synced = true;
        Serial.printf("[time] synced: %s (after %u attempt%s)\n",
                      netTime_iso8601().c_str(), attempts, attempts == 1 ? "" : "s");
        return;
    }

    // Log the actual clock, not just "not synced". If this ever prints a 1970
    // date next to a TLS certificate error, the two lines together say exactly
    // what is wrong.
    if (attempts == 1 || attempts % 10 == 0) {
        Serial.printf("[time] not synced yet (clock reads %s) — TLS is blocked until it is\n",
                      netTime_iso8601().c_str());
    }
}

bool netTime_isSynced() {
    // Re-check rather than trusting the flag: an SNTP step backwards, or a
    // clock reset, should re-block TLS rather than let it fail confusingly.
    if (synced && !clockIsSane()) {
        synced = false;
        configured = false;
        attempts = 0;
        Serial.println("[time] clock went implausible again — re-syncing, MQTT paused");
    }
    return synced;
}

String netTime_iso8601() {
    time_t t = time(nullptr);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return String(buf);
}
