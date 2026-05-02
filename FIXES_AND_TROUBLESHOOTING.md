# V2V System Fixes & Troubleshooting Guide

## 🔧 What Was Fixed

### **Problem 1: System Halts Without GPS Fix**
**Original Issue**: If GPS has no fix, vehicle never broadcasts → network is broken
**Fix**: Now transmits with 0.0 coordinates if GPS unavailable (allowing relaying)
**File Changes**: `sendOwnPacket()` in all three files

---

### **Problem 2: Proximity Alerts Blocked Without GPS**
**Original Issue**: `if (!snap.gpsFixed) { clearAlerts(); return; }` stops all processing
**Fix**: Alerts now work even without own GPS (using simple presence detection)
**File Changes**: `evaluateProximityAlerts()` in all three files

---

### **Problem 3: No Testing Mode for Development**
**Original Issue**: Can't test without real GPS outdoors
**Fix**: Added `ENABLE_SIMULATION_MODE` flag to broadcast hardcoded coordinates
**Usage**: Change this line to test without GPS:
```cpp
#define ENABLE_SIMULATION_MODE  true   // Set to true to use test data
```

---

## 🧪 Testing Steps (In Order)

### **Step 1: Run Diagnostic Test (FASTEST)**
1. Upload `DIAGNOSTIC_TEST.ino` to ONE ESP32 board
2. Open Serial Monitor (115200 baud)
3. Check output for:
   - ✓ LoRa module detected? 
   - ✓ GPS sending data?
   - ✓ Alert pins working?

**If LoRa FAILS:**
- Check: 3.3V power on LoRa module
- Check: GPIO 14 (RST) is connected (pulled high or free)
- Check: GPIO 2 (DIO0) is connected (this is the interrupt pin!)
- Try: Swap SPI pins if using different ESP32 variant

**If GPS FAILS:**
- Check: TX pin of GPS module → GPIO 26 (ESP32 RX)
- Check: Try different baud rates: 4800, 9600, 38400
- Check: GPS module has 5V or 3.3V power
- Note: GPS needs open sky (not indoors) to get fix

---

### **Step 2: Test LoRa Only (No GPS)**
1. Set in each file:
   ```cpp
   #define ENABLE_SIMULATION_MODE  true   // Use test coordinates
   ```
2. Upload to 2-3 boards
3. Open Serial Monitor on each → should see [TX V1], [TX V2], etc.
4. On second board → should see [RX V1] messages

**If communication FAILS:**
- Verify all boards use **433MHz** (not 868 or 915)
- Check SPI connections (especially MOSI/MISO/CLK)
- Check NSS pin (GPIO 5) is pulled high between packets

---

### **Step 3: Add Real GPS (Outdoors)**
1. Go outside with open sky
2. Set in each file:
   ```cpp
   #define ENABLE_SIMULATION_MODE  false   // Use real GPS
   ```
3. Upload and wait 30-60 seconds for GPS fix
4. Serial Monitor should show: `lat=12.971599 lon=77.594563`

---

### **Step 4: Test Proximity Alerts**
1. With two boards having GPS fixes:
2. Bring them closer together
3. Watch Serial Monitor for distance calculations
4. At <50m: LED fast blink + buzzer
5. At <150m: LED slow blink only

---

## ⚠️ Common Issues & Solutions

| Problem | Cause | Solution |
|---------|-------|----------|
| LoRa not detected | SPI not initialized or chip broken | Check 3.3V, RST pin, try different board |
| GPS no fix | Indoors/weak signal | Go outside, clear sky view |
| LoRa TX succeeds but no RX | Frequency mismatch or RX broken | Verify all 3 boards set to same frequency |
| Alerts never trigger | GPS sending 0.0 coords | Use ENABLE_SIMULATION_MODE or get real GPS fix |
| Random LED/buzzer glitches | Power supply ripple | Add 100µF capacitor near 3.3V rail |
| Timeouts after 3 seconds | Stale data timeout | Verify all 3 boards transmitting regularly |

---

## 📋 Changes Summary

### Files Modified:
- ✅ `v2v_1/v2v_1.ino`
- ✅ `v2v_2/v2v_2.ino` 
- ✅ `v2v_3/v2v_.ino`

### New File:
- ✅ `DIAGNOSTIC_TEST.ino` - Use this first!

### What Changed in Each File:
1. **Added simulation mode constants** (lines ~110-115):
   ```cpp
   #define ENABLE_SIMULATION_MODE  false
   #define SIM_LATITUDE            12.971599
   #define SIM_LONGITUDE           77.594563
   #define SIM_SPEED               45.0
   ```

2. **Modified `sendOwnPacket()`**: Now broadcasts even without GPS
   - Checks `ENABLE_SIMULATION_MODE` for test data
   - Sends 0.0 coordinates if GPS unavailable
   - Shows `[GPS]` or `[NO GPS]` in Serial output

3. **Modified `evaluateProximityAlerts()`**: Processes alerts without own GPS
   - Handles relay mode (broadcasting without own position)
   - Still calculates distances when GPS available
   - Triggers "presence alerts" for relay vehicles

---

## 🚀 Next Steps

1. **Upload `DIAGNOSTIC_TEST.ino`** to identify failures
2. **Fix hardware issues** (power, connections, baud rate)
3. **Test with simulation mode** to verify LoRa works
4. **Add real GPS** and test outdoors
5. **Deploy on all three boards** with correct VEHICLE_ID

---

## 📞 Quick Reference

**To test without GPS:**
```cpp
#define ENABLE_SIMULATION_MODE  true
```

**To test with real GPS:**
```cpp
#define ENABLE_SIMULATION_MODE  false
```

**Expected Serial Output (Simulation Mode):**
```
[TX  V1] 1,12.971599,77.594563,45.00,123456  [GPS]
[RX  V2] lat=12.971599 lon=77.594563 spd=45.0 km/h  RSSI=-95 SNR=8.5
[DIST] V1→V2  =  500.5 m  [GPS OK]
[OWN  V1] lat=12.971599  lon=77.594563  spd=45.0 km/h
```

---

## ✅ Verification Checklist

- [ ] Diagnostic test shows LoRa online
- [ ] Diagnostic test shows GPS responding (or "NO DATA")
- [ ] All 3 boards transmit at 433MHz
- [ ] At least 2 boards can receive from each other
- [ ] LED + Buzzer test pulses work
- [ ] Serial Monitor shows vehicle data every 2 seconds
- [ ] GPS fix acquired outdoors
- [ ] Proximity alerts trigger within threshold distances
