# BleMqttRelays

ESP32 firmware for a 4-relay actuator node, reachable over Bluetooth and MQTT.

Target board: **`Hardware/kicad/actuator-node-Ble`**. The pin map in
`src/boardConfig.cpp` is derived from `ble_relay.py` / `ble_relay.net`; do not
change one without the other.

## Features

- BLE server: a read/write/**notify** characteristic per relay, plus a packed
  4-byte status characteristic covering all four channels
- MQTT relay commands, with retained **measured** state publishing
- **Relay coil feedback** — the board reads the true state of each coil, so the
  reported state is correct even when the on-board slider is driving the relay
- WiFi configuration portal using IotWebConf (Farsi captive portal), with a
  farm-owner name configurable through the portal
- Relay commands persisted across reboots (Preferences/NVS)
- ESP32 task watchdog **and** the board's external TLC555 hardware watchdog
- MQTT-triggered OTA firmware updates, with a randomized delay so a fleet of
  devices doesn't all update at once

## Relay coil feedback

Each relay channel has an MT-102 SPDT ON-OFF-ON toggle that selects the FET
gate source. The centre position is **open**, not grounded — the relay is held
off by `R_GPD`, the 22k gate pulldown:

| Lever | Gate driven by | Result |
|---|---|---|
| ALWAYS ON | +3V3 | relay energized, ESP32 ignored |
| OFF (centre) | nothing — `R_GPD` pulls the gate down | relay released, ESP32 ignored |
| AUTO | `RLn_IO` from the ESP32 | relay follows commands |

Because OFF is the centre detent, moving between ALWAYS ON and AUTO always
passes through OFF, so the coil drops out on the way rather than slamming
between two driven states. It also means `R_GPD` is the only thing defining
OFF — if a board ever comes back with a channel that will not release in the
centre position, check that resistor before suspecting the firmware.

An 18k/20k divider on the coil's low side feeds `SENn` (GPIO34/35/36/39, all
input-only ADC1 channels). Energized coil pulls the FET drain to ~0 V; released
coil sits at ~2.63 V. **The sense is inverted: LOW means the relay is ON.**
(The protoboard rig uses a hand-wired 10k/20k on different pins, giving ~3.33 V
and read digitally — see `boardConfig.cpp`.)

The firmware polls all four every 50 ms with hysteresis and a 3-sample debounce,
and reports three things per channel:

- **state** — what the coil is actually doing
- **cmd** — what the firmware last commanded
- **mode** — `auto` when they agree, `override_on` / `override_off` when the
  relay has been ignoring commands for more than 400 ms, `unknown` when the
  sense voltage sits between thresholds

A sustained mismatch usually means a slider was left off AUTO, but a failed FET,
an open coil or a broken sense resistor look identical from the ESP32's side —
hence `override_*` names the symptom, not the cause.

## Relay MQTT topics

`<module>` is the role-addressed prefix (e.g. `rmhc1`); `farmOwner` is set
through the WiFi captive portal.

| Topic | Direction | Payload |
|---|---|---|
| `actuator/<farmOwner>/<module>/rl0N/on` | in | command relay N on |
| `actuator/<farmOwner>/<module>/rl0N/off` | in | command relay N off |
| `actuator/<farmOwner>/<module>/rl0N/state` | out, retained | **measured** state: `1` off, `2` on |
| `actuator/<farmOwner>/<module>/rl0N/cmd` | out, retained | commanded state: `1` off, `2` on |
| `actuator/<farmOwner>/<module>/rl0N/mode` | out, retained | `auto` \| `override_on` \| `override_off` \| `unknown` |
| `actuator/<farmOwner>/<module>/online` | out, retained | `2` online, `1` offline (broker Last-Will) |

All three relay topics are re-asserted every 60 s so a stale retained value
cannot outlive reality.

> **Migration note:** `state` previously carried the *commanded* value. Anything
> reading it (Node-RED flows, Grafana panels) now sees measured state. That is
> almost always what was wanted, but flows that echoed `state` back as a command
> must be checked — they would now fight a slider override.

The role status topics `sensors/<owner>/ts<A>/{hOn,cOn}` and
`sensors/<owner>/hs<A>/hOn` also report measured rather than intended state.

## BLE

One service. Per relay: one characteristic with READ / WRITE / WRITE_NR /
NOTIFY. Writing `0x01` commands the relay ON, any other byte OFF; reading and
notifications return the **measured** state.

One extra characteristic carries all four channels packed, one byte per relay:

| Bit | Meaning |
|---|---|
| 0 | actual (measured coil state) |
| 1 | commanded |
| 2-3 | mode: 0 auto, 1 override_on, 2 override_off, 3 unknown |

Its UUID follows the per-relay pattern with `0` in place of the relay number:
`beb5483e-36e1-<dip>80-b7f5-<owner>`. Service and characteristic UUIDs are
derived from `farmOwner` and the 8-bit DIP value so multiple boards don't
collide — see the BLE identity caveat in Notes.

## Hardware watchdog

`HWD_HB` (GPIO16) drives a TLC555 with R = 9M1 and C = 1uF, giving roughly a
**10 second** timeout. Idle HIGH lets the timing cap charge; a LOW pulse both
holds the 555 trigger low and turns on the BC807 that dumps the cap.

`hwWatchdog.cpp` kicks it from a dedicated FreeRTOS task every 2 s, not from
`loop()`, because WiFi association, MQTT connect and OTA downloads all block
`loop()` for longer than the timeout.

**Fit the `JP_WD` jumper before flashing over serial** — it parks the watchdog.

## Status LEDs

| LED | Pattern | Meaning |
|---|---|---|
| `LED_STATUS` (GPIO2) | solid | MQTT connected |
| | slow blink | WiFi up, broker unreachable |
| | fast double-blink | a relay is not following commands |
| | off | no WiFi |
| `LED_BLE` (GPIO12) | solid | BLE central connected |
| | short blink | advertising |

## OTA updates

The device subscribes to `configs/ts/<model>/<hardware-version>/firmVer` and,
when it sees a version newer than the one stored in NVS, downloads
`http://asanautomation.ir/uploads/firmware/BleMqttRelays.ino_<model>_<hardware-version>.<version>.bin`
after a random delay (up to 5 minutes). Adjust `OTA_MODEL_CODE` /
`OTA_HARDWARE_VERSION` in `src/otaUpdater.h` to match your naming.

## Build

- Use PlatformIO in the project folder

## Notes

- Adjust `MQTT_SERVER`, `MQTT_PORT`, and credentials in `src/mqttFunctions.cpp`.
  The broker account is currently shared between the app and every board; see
  `Docs/Architecture/Multi-Farm-and-User-Roles-PLAN.md` §0.2.
- GPIO12 (`LED_BLE`) and GPIO2 (`LED_STATUS`) are strapping pins, but both drive
  a resistor-and-LED load to ground, so neither is pulled high at reset. Safe.
- GPIO15 carries `DIP3` and has a 10k pull-up. With DIP3 ON the pin is low at
  boot, which silences the ROM bootloader log — cosmetic, but surprising when
  debugging.
- The `SENn` divider on this board is **18k/20k → 2.63 V** when a relay is
  released, comfortably under the 3.3 V rail. (It was 10k/20k → 3.33 V, which
  sat above the rail; that was fixed by raising `R_ST`, not by lowering `R_SB`
  as an earlier note here suggested.) Analog thresholds `RELAY_FB_MV_ON_MAX`
  1000 mV and `RELAY_FB_MV_OFF_MIN` 1900 mV both sit clear of it.
  **Do not switch this board to `digitalRead()`** — at 2.63 V against a VIH of
  0.75 × VDD the margin is roughly 150 mV, and none at the top of rail
  tolerance. The protoboard variant reads digitally, but on a different
  hand-wired divider; see `boardConfig.cpp`.
- `R_GPD` (22k, gate pulldown) is what holds a relay off when its MT-102 is in
  the centre position — the centre is open, not grounded. It is a
  safety-critical part on this board, not an optional pulldown.
- BLE identity truncates the owner name to 6 characters, so two owners sharing a
  6-character prefix produce the same UUIDs. Pre-existing; see the plan doc §2.4.
