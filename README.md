# LoRa GPS Vehicle Communication System

A Vehicle-to-Vehicle (V2V) prototype built with ESP32, SX1278 (Ra-02), and NEO-6M GPS.

## Project Goal
Build a decentralized, internet-independent safety communication layer where each vehicle shares position and motion data over LoRa to support collision warning logic.

## Current Status
- ESP32 setup: Complete
- LoRa communication (433 MHz): Complete
- GPS UART data acquisition: Complete
- End-to-end link (GPS -> TX -> LoRa -> RX -> Serial): Working
- Structured data protocol (JSON): Not implemented yet
- Collision logic and alerts: Not implemented yet
- Multi-node bidirectional behavior: Not implemented yet

## Workspace Structure
- `lora_gps_test/`
  - `lora_gps_test_transmitter/`: GPS + LoRa transmitter sketch
  - `lora_gps_test_receiver/`: LoRa receiver sketch
- `lora_test/`
  - `lora_test_01/`: Basic LoRa transmitter test
  - `lora_test_02/`: Basic LoRa receiver test

## Hardware
- ESP32 Dev Module
- SX1278 (Ra-02) LoRa module (433 MHz)
- NEO-6M GPS module

## LoRa Pin Mapping (Current Sketches)
- SS: GPIO 5
- RST: GPIO 14
- DIO0: GPIO 2

## GPS UART Mapping (Current TX Sketch)
- RX: GPIO 16
- TX: GPIO 17
- Baud: 9600

## Next Updates Planned
1. JSON packet format with vehicle ID and timestamp
2. GPS fix validation and clean field extraction
3. Bidirectional send/receive loop per node
4. Distance and relative-speed risk metrics
5. Collision warning logic with LED/Buzzer outputs

## Upload and Monitor
Compile and upload sketches using Arduino IDE or PlatformIO.
Use Serial Monitor at 115200 baud for all current sketches.

## License
Choose a license before public release (MIT is common for embedded prototypes).
