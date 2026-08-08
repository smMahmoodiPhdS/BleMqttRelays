#!/usr/bin/env bash
# Host-side tests for BleMqttRelays. No board, no PlatformIO, no network.
#
# Compiles the real sources against small Arduino stubs (stub/) and asserts the
# reconnect-timer behaviour plus the topic/retain contract. Companion to
# ../../../bleMqttSensor/test/host/run.sh, which shares the same stubs.
#
# WHY: `pio run` needs the espressif32 toolchain, which is not always reachable
# from a CI sandbox. This catches contract and control-flow regressions in two
# seconds. It cannot see a wrong GPIO - only a board can.
set -euo pipefail
cd "$(dirname "$0")"

SRC=../../src
# MUST match the real toolchain. arduino-esp32 2.0.6 (espressif32@5.4.0)
# compiles with -std=gnu++11, and the difference is not academic: a struct with
# default member initialisers is an aggregate from C++14 but NOT in C++11, so
# `MyStruct s = {a, b, c};` compiles here under gnu++17 and fails on the board
# with "no match for operator=". That exact bug shipped once.
#
# Same principle as the Arduino stubs: a harness more permissive than the real
# build does not merely miss errors, it certifies broken code. If you raise this,
# raise it in platformio.ini too, or not at all.
CXXFLAGS=(-std=gnu++11 -Wall -Wextra -Wno-unused-parameter -I stub -I shim)
FAIL=0

# The real wifiManagerTools.h pulls IotWebConf, which is not stubbed. Quoted
# includes resolve from the including file's directory first, so it is shadowed by
# copying the sources to a scratch dir without it.
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
cp -r "$SRC" "$WORK/src"
rm -f "$WORK/src/wifiManagerTools.h" "$WORK/src/wifiManagerTools.cpp"
sed -i 's|#include <time.h>|#include <time.h>\n#include "esp_sntp_stub.h"|' "$WORK/src/netTime.cpp"

echo "== syntax, both board variants =="
for v in BOARD_PROTO_4_6 BOARD_ACTUATOR_BLE; do
  for f in mqttFunctions sensorSim boardConfig netTime otaUpdater; do
    if out=$(g++ "${CXXFLAGS[@]}" -D "$v" -I "$WORK/src" -fsyntax-only "$WORK/src/$f.cpp" 2>&1) \
       && [ -z "$out" ]; then printf '  OK    %-18s %s\n' "$f.cpp" "$v"
    else printf '  FAIL  %-18s %s\n' "$f.cpp" "$v"; echo "$out" | head -12; FAIL=1; fi
  done
done

echo
echo "== reconnect timer + topic assertions =="
g++ "${CXXFLAGS[@]}" -D BOARD_PROTO_4_6 -I "$WORK/src" -o "$WORK/assert" \
  assert_actuator.cpp deps_stub.cpp globals.cpp wifiManagerTools_stub.cpp \
  "$WORK/src/mqttFunctions.cpp" "$WORK/src/sensorSim.cpp" \
  "$WORK/src/boardConfig.cpp" "$WORK/src/netTime.cpp" "$WORK/src/otaUpdater.cpp"
"$WORK/assert" || FAIL=1

echo
[ "$FAIL" -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
