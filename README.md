# BleMqttRelays

ESP32 firmware for controlling 4 relays via Bluetooth and MQTT.

## Features

- BLE server with a read/write characteristic per relay
- MQTT subscription for relay on/off commands, retained state publish
- WiFi configuration portal using IotWebConf (Farsi captive portal), with a
  farm-owner name configurable through the portal
- Relay state persisted across reboots (Preferences/NVS)
- ESP32 task watchdog
- MQTT-triggered OTA firmware updates, with a randomized delay so a fleet of
  devices doesn't all update at once

## Relay MQTT topics

`farmOwner` is the name set through the WiFi captive portal.

- `actuator/<farmOwner>/rl0N/on` -> turn relay N on (N = 1-4)
- `actuator/<farmOwner>/rl0N/off` -> turn relay N off
- `actuator/<farmOwner>/rl0N/state` -> published (retained), payload `1` (off) or `2` (on)
- `actuator/<farmOwner>/online` -> device presence (retained); `2` = online (published on connect), `1` = offline (set by the broker via Last-Will on drop)

## BLE

One service, one characteristic per relay (4 total). Writing `0x01` turns
the relay ON, any other byte turns it OFF; reading returns current state.
Service/characteristic UUIDs are derived from `farmOwner` and the board's
8-bit DIP switch value, so multiple boards don't collide.

## OTA updates

The device subscribes to `configs/ts/<model>/<hardware-version>/firmVer` and,
when it sees a version newer than the one stored in NVS, downloads
`http://asanautomation.ir/uploads/firmware/BleMqttRelays.ino_<model>_<hardware-version>.<version>.bin`
after a random delay (up to 5 minutes). Adjust `OTA_MODEL_CODE` /
`OTA_HARDWARE_VERSION` in `src/otaUpdater.h` to match your naming.

## Build

- Use PlatformIO in the project folder

## Notes

- Adjust `MQTT_SERVER`, `MQTT_PORT`, and credentials in `src/mqttFunctions.cpp`
- Relay/DIP switch GPIO pins are defined in `src/boardConfig.cpp`. GPIO12 is a
  flash-voltage strapping pin used here for a DIP switch input — verify it
  doesn't interfere with boot on your flash chip.
