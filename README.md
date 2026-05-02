# Vehicle-to-Vehicle (V2V) Communication System

**A real-time proximity alert system using ESP32, LoRa, and GPS for vehicle safety networks.**

---

## 📋 Project Overview

This V2V Communication System enables three ESP32-based vehicles to broadcast their GPS positions over a 433MHz LoRa network and detect proximity between vehicles. When vehicles come within a danger threshold, the system triggers visual (LED) and audio (buzzer) alerts.

**Key Features:**
- ✅ Real-time GPS positioning (NEO-6M module)
- ✅ Long-range LoRa communication (433MHz, SF7)
- ✅ Dual-core FreeRTOS task management
- ✅ RSSI-based close-range detection
- ✅ GPS-based distance calculation (Haversine formula)
- ✅ LED and buzzer alert system
- ✅ Multi-vehicle coordination (3-node network)

---

## ⚙️ Hardware Configuration

### Components:
- **ESP32 Dev Module** (Arduino IDE compatible)
- **LoRa SX1276 Module** (433 MHz)
- **NEO-6M GPS Module** (9600 baud, 5V)
- **LED** with 220Ω resistor
- **Active Buzzer** (5V)

### Pin Connections:

| Component | Pin | GPIO |
|-----------|-----|------|
| **LoRa SX1276** | | |
| - SCK | SPI | GPIO 18 |
| - MISO | SPI | GPIO 19 |
| - MOSI | SPI | GPIO 23 |
| - NSS | CS | GPIO 5 |
| - RST | Reset | GPIO 14 |
| - DIO0 | IRQ | GPIO 2 |
| **GPS NEO-6M** | | |
| - TX | RX | GPIO 35 ⭐ |
| - RX | TX | Not used (-1) |
| - VCC | Power | 5V/VIN (critical!) |
| **Alerts** | | |
| - LED | Anode | GPIO 4 |
| - Buzzer | Positive | GPIO 15 |

---

## 🔧 Configuration

### For Each Vehicle, Change ONE Line:

```cpp
#define VEHICLE_ID  1    // Vehicle 1
// #define VEHICLE_ID  2    // Vehicle 2
// #define VEHICLE_ID  3    // Vehicle 3
```

### Proximity Thresholds:

```cpp
#define DIST_DANGER_M       1200.0  // < 1200 cm (12 m)  → Danger (buzzer ON)
#define DIST_WARNING_M      1500.0  // < 1500 cm (15 m)  → Warning (slow blink)
#define RSSI_DANGER_THRESHOLD -27   // Signal strength check for close range
```

### Optional Test Mode:

```cpp
#define ENABLE_SIMULATION_MODE  false   // Set true for demo without real GPS
```

---

## 🚀 Getting Started

### 1. Install Libraries (Arduino IDE)

Sketch → Include Library → Manage Libraries:
- **LoRa** by Sandeep Mistry
- **TinyGPSPlus** by Mikal Hart

### 2. Configure Board

- Board: ESP32 Dev Module
- Port: COM4 (or your serial port)
- Upload Speed: 115200 baud

### 3. Upload Sketches

Upload the appropriate sketch to each board:
- `v2v_1/v2v_1.ino` → Board 1 (VEHICLE_ID=1)
- `v2v_2/v2v_2.ino` → Board 2 (VEHICLE_ID=2)
- `v2v_3/v2v_3.ino` → Board 3 (VEHICLE_ID=3)

### 4. Test System

Open Serial Monitor (115200 baud) on each board:
- Watch for GPS fix acquisition
- Verify LoRa packet exchange (TX/RX messages)
- Test alerts by moving boards within 12-15 meters

---

## 📊 System Output

### Startup Banner:
```
╔════════════════════════════════════════════╗
║   V2V Communication System  — ESP32/LoRa   ║
║   Vehicle-to-Vehicle Safety Network        ║
╠════════════════════════════════════════════╣
║   This node: Vehicle ID  =  1              ║
║   LoRa Freq: 433 MHz                       ║
║   Danger threshold : 1200 cm (12 m)        ║
║   Warning threshold: 1500 cm (15 m)        ║
╚════════════════════════════════════════════╝
```

### Live Monitoring:
```
[OWN  V1] lat=13.114654 lon=77.633576 spd=0.4 km/h
[PEER V2] lat=13.114630 lon=77.633475 spd=1.2 km/h  age=199ms
[TX  V1] 1,13.114654,77.633576,0.46,56860  [GPS]
[RX  V2] lat=13.114654 lon=77.633573 spd=0.3 km/h  RSSI=-26 SNR=10.2
[DIST] V1→V2  =  1124.1 cm  [GPS OK]
[RSSI] V1→V2 = -26 dBm (Very Close!) — DANGER  ← Buzzer ON!
```

---

## 🐛 Troubleshooting Journey

### Issue #1: GPS Not Acquiring Fix ❌ → ✅ FIXED

**Symptoms:**
- GPS module blinking but Serial shows "Waiting for GPS fix..."
- Coordinates always 0.0, 0.0

**Root Cause:**
- GPIO 26 RX pin on ESP32 board was **physically damaged** (hardware fault)
- Voltage divider attempts failed because the pin couldn't accept any signal

**Solution:**
- Switched GPS RX from GPIO 26 to GPIO 35 (input-only pin)
- GPIO 35 natively accepts 5V signals without voltage divider
- Updated all three files to use `HardwareSerial gpsSerial(1)` on GPIO 35

**Code Change:**
```cpp
// BEFORE (broken):
#define GPS_RX_PIN  26
HardwareSerial gpsSerial(2);  // UART2

// AFTER (working):
#define GPS_RX_PIN  35
HardwareSerial gpsSerial(1);  // UART1
```

---

### Issue #2: GPS Data Corrupted / Invalid Sentences ❌ → ✅ FIXED

**Symptoms:**
- GPS module transmitting data (module blinking, characters in Serial)
- TinyGPSPlus parsing 0 valid sentences
- All coordinates remain 0.0

**Root Cause:**
- GPS TX outputs 5V, ESP32 GPIO 26 expects 3.3V
- Voltage levels incompatible, causing data corruption

**Failed Attempts:**
- ❌ 1kΩ voltage divider: Still failed (GPIO 26 hardware damaged)
- ❌ 220Ω voltage divider: Partial success but unreliable

**Solution:**
- Use GPIO 35 (input-only, handles 5V natively)
- No voltage divider needed

---

### Issue #3: System Blocking Without GPS Fix ❌ → ✅ FIXED

**Symptoms:**
- Vehicle with no GPS fix would not transmit
- Other vehicles wouldn't receive alerts if one vehicle had no GPS
- Entire system appeared broken when GPS unavailable

**Root Cause:**
- Original code had GPS fix requirement before transmitting:
```cpp
if (!snap.gpsFixed) {
    clearAlerts();
    return;  // ← Blocked everything!
}
```

**Solution:**
- Implemented **relay/presence mode**
- Vehicle broadcasts 0.0 coordinates even without GPS fix
- Other vehicles still detect presence via LoRa signal
- Proximity alerts work based on RSSI (signal strength)

**Code Change:**
```cpp
// NOW: Always transmit (even with 0.0 if no GPS fix)
// This allows relay vehicles to participate
if (ENABLE_SIMULATION_MODE) {
    snap.latitude = SIM_LATITUDE;
    snap.longitude = SIM_LONGITUDE;
} else {
    // Transmit with 0.0 if no GPS, or real coords if available
}
LoRa.beginPacket();
LoRa.print(packet);  // ← Always succeeds
```

---

### Issue #4: Buzzer Not Triggering at 10-15cm Distance ❌ → ✅ FIXED

**Symptoms:**
- Boards placed 5cm apart, but buzzer silent
- LEDs not blinking
- Distance calculations showed 250-400cm between boards that were touching

**Root Cause #1:**
- GPS accuracy is only ±2.5 meters in ideal conditions
- Cannot distinguish between 5cm and 5 meters
- All boards appeared at 250+ cm away even when side-by-side

**Root Cause #2:**
- Threshold was set to 100cm, but real-world distances were 1000-1900cm

**Solutions Implemented:**

1. **Added RSSI-based detection** (short range):
   - When RSSI < -27 dBm, devices are at 10-15 meters
   - Triggers DANGER immediately without waiting for GPS
   
2. **Adjusted GPS thresholds** (medium range):
   - Changed DIST_DANGER_M from 100cm to 1200cm (12m)
   - Changed DIST_WARNING_M from 400cm to 1500cm (15m)
   - Now matches actual field distances

3. **Two-stage detection system**:
   ```
   First Check: Is RSSI < -27 dBm? → YES → DANGER (buzzer ON) ✅
   Second Check: Is GPS distance < 1200cm? → YES → DANGER (buzzer ON) ✅
   Third Check: Is GPS distance < 1500cm? → YES → WARNING (slow blink)
   ```

**Code Evolution:**
```cpp
// ITERATION 1: Too strict (never triggered)
#define DIST_DANGER_M  100.0   // 1 meter only

// ITERATION 2: Simulation mode (unrealistic)
#define ENABLE_SIMULATION_MODE true  // Same coordinates everywhere

// ITERATION 3: Real GPS + RSSI backup (WORKING!)
#define DIST_DANGER_M 1200.0   // 12 meters (real-world range)
#define RSSI_DANGER_THRESHOLD -27  // Signal strength check
```

---

### Issue #5: LoRa Communication Inconsistent ❌ → ✅ FIXED

**Symptoms:**
- Occasional packet loss
- Unreliable RX of peer data

**Root Cause:**
- Initial LoRa configuration used suboptimal settings
- Spreading Factor and Bandwidth not optimized

**Solution:**
- Set SF7 (good balance of range/speed)
- BW 125kHz (standard ISM band)
- CRC enabled for error checking
- TX power 17dBm (legal limit, good range)

```cpp
LoRa.setSpreadingFactor(7);        // SF7
LoRa.setSignalBandwidth(125E3);    // 125 kHz
LoRa.setCodingRate4(5);            // CR4/5
LoRa.enableCrc();                  // Drop corrupted packets
LoRa.setTxPower(17);               // 17 dBm
```

---

## 📈 Final System Performance

| Metric | Status |
|--------|--------|
| GPS Acquisition | ✅ Working (2-5 min outdoor) |
| LoRa Communication | ✅ Reliable (RSSI -26 to -33 dBm) |
| Proximity Detection | ✅ Working (12-15m range) |
| Alert System | ✅ LED + Buzzer triggering |
| Multi-Vehicle | ✅ 3 boards communicating |
| Dual-Core FreeRTOS | ✅ Stable (no crashes) |

---

## 📁 Project Structure

```
V2V/
├── v2v_1/
│   └── v2v_1.ino          (Vehicle 1 firmware - VEHICLE_ID=1)
├── v2v_2/
│   └── v2v_2.ino          (Vehicle 2 firmware - VEHICLE_ID=2)
├── v2v_3/
│   └── v2v_3.ino          (Vehicle 3 firmware - VEHICLE_ID=3)
└── README.md              (This file)
```

---

## 🎯 Key Lessons Learned

1. **GPIO Damage Detection:**
   - Not all GPIO pins are created equal
   - GPIO 26 on this ESP32 board was damaged (common issue)
   - GPIO 35 (input-only) worked perfectly for GPS RX

2. **GPS Accuracy Limitations:**
   - ±2.5m accuracy standard for consumer GPS
   - Cannot reliably measure cm-level distances
   - RSSI signal strength is more reliable for close range

3. **Dual-Core Processing:**
   - Core 0 handles LoRa (real-time requirements)
   - Core 1 handles GPS + alerts (timing flexible)
   - Mutex protection essential for shared data

4. **LoRa vs GPS Trade-offs:**
   - LoRa: Great range (15+ km), poor accuracy
   - GPS: Accurate position (±2.5m), requires satellites
   - Hybrid approach: Use RSSI for close-range, GPS for far-range

5. **Field Testing is Critical:**
   - Simulator shows optimal conditions
   - Real world: GPS jitter, RSSI fluctuation, environmental noise
   - Thresholds must be tuned to actual hardware

---

## 🔐 Safety Considerations

- ⚠️ GPS requires clear sky view (outdoor, not under trees/roofs)
- ⚠️ LoRa has 15+ km range - adjust thresholds for your use case
- ⚠️ Buzzer is loud (protect hearing near ears)
- ⚠️ System should not be sole safety mechanism

---

## 📝 License

This project is provided as-is for educational and research purposes.

---

## 👤 Contributors

**Development Team:**
- Pre Final Year Engineering Project
- Developed with Arduino IDE + FreeRTOS
- Debugged and optimized for real-world conditions

---

## 🙏 Acknowledgments

Special thanks to the open-source community:
- Sandeep Mistry for LoRa library
- Mikal Hart for TinyGPSPlus
- Espressif Systems for ESP32 board support

---

**Last Updated:** May 2, 2026  
**Status:** ✅ Fully Functional - Ready for Deployment
