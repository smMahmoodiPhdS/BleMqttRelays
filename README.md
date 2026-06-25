# BleMqttRelays

Simple ESP32 firmware for controlling 4 relays via Bluetooth and MQTT.

## Features

- BLE server with a write characteristic for relay commands
- MQTT subscription for relay set/toggle commands
- WiFi configuration portal using IotWebConf

## Relay topics

- `blemqtt/relays/N/set` -> payload `0` or `1`
- `blemqtt/relays/N/toggle` -> payload ignored
- `blemqtt/relays/N/state` -> published state `0` or `1`

## Build

- Use PlatformIO in the project folder

## Notes

- Adjust `mqttServer`, `mqttPort`, and credentials in `src/main.cpp`
