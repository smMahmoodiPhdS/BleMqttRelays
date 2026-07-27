#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// NTP time, and why MQTT must wait for it.
//
// TLS certificate validation compares the certificate's notBefore/notAfter
// against the device clock. An ESP32 boots at epoch 0 — 1 January 1970 — so
// every certificate on earth reads as "not yet valid" and the handshake fails.
//
// The failure is nasty because it lies about its cause: the error surfaces as a
// certificate problem, on a certificate that is perfectly valid, against a
// broker that is perfectly reachable. Hours get spent re-issuing certs and
// checking chains. The clock is the bug.
//
// So: sync first, and refuse to attempt MQTT until the year looks like a year.
// ---------------------------------------------------------------------------

// Anything before 2024 means NTP has not landed yet. Not a precise bound —
// just far enough past 1970 to be unambiguous.
#define NTP_SANE_EPOCH 1704067200UL   // 2024-01-01T00:00:00Z

void setup_netTime();

// Non-blocking: call from loop(). Retries on a backoff until the clock is sane.
void netTime_loop();

// True once the clock is plausible. The MQTT layer gates on this.
bool netTime_isSynced();

// UTC, human readable, for logs.
String netTime_iso8601();
