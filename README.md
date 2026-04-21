# esp32-relay
runs my shit
# esp32-relay

ESP32 firmware that connects to WiFi, subscribes to an MQTT broker, and
pulses an output pin for 1 second when commanded. Supports remote OTA
updates via GitHub Releases triggered over MQTT.

## Hardware

- Any ESP32 dev board
- Output: GPIO 27 (configurable in the sketch)

## Local setup

1. Clone the repo.
2. Copy the secrets template: