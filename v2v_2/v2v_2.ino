/*
 * ============================================================================
 *  Vehicle-to-Vehicle (V2V) Communication System
 *  Using ESP32 + LoRa SX1276 + NEO-6M GPS
 * ============================================================================
 *  Project   : V2V Communication Using ESP32, LoRa, and GPS
 *  Author    : Final Year Engineering Project
 *  Target    : ESP32 Dev Module (Arduino IDE)
 *  Board Pkg : esp32 by Espressif Systems (v2.x)
 *  Libraries : LoRa by Sandeep Mistry, TinyGPSPlus by Mikal Hart
 * ============================================================================
 *
 *  HOW TO CONFIGURE FOR EACH VEHICLE:
 *  ------------------------------------
 *  Change only ONE line before uploading to each board:
 *
 *    #define VEHICLE_ID   1     ← Vehicle 1
 *    #define VEHICLE_ID   2     ← Vehicle 2
 *    #define VEHICLE_ID   3     ← Vehicle 3
 *
 * ============================================================================
 *
 *  HARDWARE CONNECTIONS (verified):
 *  ----------------------------------
 *  LoRa SX1276:
 *    VCC  → 3.3V       GND  → GND
 *    SCK  → GPIO 18    MISO → GPIO 19
 *    MOSI → GPIO 23    NSS  → GPIO 5
 *    RST  → GPIO 14    DIO0 → GPIO 2
 *
 *  NEO-6M GPS:
 *    VCC  → 5V/VIN     GND  → GND
 *    TX   → GPIO 26    RX   → GPIO 27
 *
 *  Alerts:
 *    Buzzer (+) → GPIO 15    LED Anode → GPIO 4
 *    Buzzer (-) → GND        LED Cathode → GND
 *
 * ============================================================================
 *
 *  DUAL-CORE TASK ASSIGNMENT:
 *  ---------------------------
 *    Core 0  →  taskLoRaComm()    — LoRa TX / RX via FreeRTOS
 *    Core 1  →  taskGPSAndAlerts() — GPS read, speed calc, proximity alerts
 *
 * ============================================================================
 *
 *  PACKET FORMAT (CSV over LoRa):
 *  --------------------------------
 *    "VID,LAT,LON,SPEED,TIMESTAMP\n"
 *    Example: "1,12.971599,77.594563,45.3,123456\n"
 *
 * ============================================================================
 */

// ─────────────────────────────────────────────────────────────────────────────
//  LIBRARY INCLUDES
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>           // Sandeep Mistry's LoRa library
#include <TinyGPSPlus.h>    // Mikal Hart's TinyGPSPlus
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ─────────────────────────────────────────────────────────────────────────────
//  *** CHANGE THIS VALUE FOR EACH VEHICLE BEFORE UPLOADING ***
// ─────────────────────────────────────────────────────────────────────────────
#define VEHICLE_ID          2       // 1, 2, or 3

// ─────────────────────────────────────────────────────────────────────────────
//  PIN DEFINITIONS
// ─────────────────────────────────────────────────────────────────────────────

// LoRa SX1276
#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23
#define LORA_SS             5
#define LORA_RST            14
#define LORA_DIO0           2

// GPS NEO-6M (via UART1 on GPIO 35 - input only pin)
// GPIO 35 works perfectly! GPIO 26 was the problem
#define GPS_RX_PIN          35      // GPS TX → ESP32 RX (works!)
#define GPS_TX_PIN          -1      // Not used (GPIO 35 is RX-only)
#define GPS_BAUD            9600

// Alert Outputs
#define PIN_BUZZER          15
#define PIN_LED             4

// ─────────────────────────────────────────────────────────────────────────────
//  LoRa RADIO SETTINGS
// ─────────────────────────────────────────────────────────────────────────────
#define LORA_FREQUENCY      433E6   // 433 MHz (change to 868E6 or 915E6 if needed)
#define LORA_TX_POWER       17      // dBm (2–20 dBm)
#define LORA_BANDWIDTH      125E3   // 125 kHz
#define LORA_SPREAD_FACTOR  7       // SF7 — good range/speed balance

// ─────────────────────────────────────────────────────────────────────────────
//  DEBUG / TEST MODE
// ─────────────────────────────────────────────────────────────────────────────
#define ENABLE_SIMULATION_MODE  false   // Use real GPS
#define SIM_LATITUDE            12.971599
#define SIM_LONGITUDE           77.594563
#define SIM_SPEED               45.0
#define LORA_CODING_RATE        5       // 4/5

// ─────────────────────────────────────────────────────────────────────────────
//  RSSI-BASED CLOSE-RANGE DETECTION (for 10-15cm proximity)
// ─────────────────────────────────────────────────────────────────────────────
#define RSSI_DANGER_THRESHOLD   -27     // RSSI < -27 dBm = close range (10-15m)

// ─────────────────────────────────────────────────────────────────────────────
//  TIMING CONSTANTS  (milliseconds)
// ─────────────────────────────────────────────────────────────────────────────
#define TX_INTERVAL_MS      500     // How often this vehicle broadcasts
#define GPS_UPDATE_MS       200     // GPS polling interval
#define ALERT_TIMEOUT_MS    3000    // Stop alerting if no update for 3 s
#define LED_BLINK_FAST_MS   150     // Fast blink period during collision warning
#define LED_BLINK_SLOW_MS   500     // Slow blink when vehicle nearby

// ─────────────────────────────────────────────────────────────────────────────
//  PROXIMITY THRESHOLDS  (centimetres)
// ─────────────────────────────────────────────────────────────────────────────
#define DIST_DANGER_M       1200.0  // < 1200 cm (12 m)  → danger (fast blink + buzzer)
#define DIST_WARNING_M      1500.0  // < 1500 cm (15 m)  → warning (slow blink)

// ─────────────────────────────────────────────────────────────────────────────
//  SYSTEM CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────
#define NUM_OTHER_VEHICLES  2       // Always 2 peers in a 3-node network
#define MAX_PACKET_LEN      80      // Max expected LoRa packet bytes
#define SERIAL_BAUD         115200

// ─────────────────────────────────────────────────────────────────────────────
//  DATA STRUCTURES
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief  Holds the last known state of one remote vehicle.
 */
struct VehicleData {
    uint8_t  id;                // Vehicle ID (1–3)
    double   latitude;          // Decimal degrees
    double   longitude;         // Decimal degrees
    float    speed;             // km/h
    uint32_t timestamp;         // millis() at time of receipt
    bool     valid;             // True once at least one packet received
    int      lastRssi;          // RSSI in dBm (signal strength, -120 to 0)
};

/**
 * @brief  Snapshot of THIS vehicle's current state, shared between cores.
 */
struct OwnData {
    double   latitude;
    double   longitude;
    float    speed;             // km/h (from GPS)
    bool     gpsFixed;          // True when GPS has a valid fix
};

// ─────────────────────────────────────────────────────────────────────────────
//  GLOBAL OBJECTS
// ─────────────────────────────────────────────────────────────────────────────
TinyGPSPlus          gps;
HardwareSerial       gpsSerial(1);          // UART1 for GPS (GPIO 35)

// Peer vehicle state table (index 0 = peer with smallest ID, etc.)
VehicleData          peers[NUM_OTHER_VEHICLES];

// This vehicle's live data (written by Core 1, read by Core 0)
OwnData              ownData   = {0.0, 0.0, 0.0, false};

// Mutex to protect shared data between cores
SemaphoreHandle_t    dataMutex = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
//  FORWARD DECLARATIONS
// ─────────────────────────────────────────────────────────────────────────────
void     initHardware();
bool     initLoRa();
bool     initGPS();
void     initAlertPins();
void     printStartupBanner();
void     printHardwareStatus(bool loraOK, bool gpsOK);

// FreeRTOS task functions
void     taskLoRaComm(void* pvParams);      // Core 0
void     taskGPSAndAlerts(void* pvParams);  // Core 1

// LoRa helpers
void     sendOwnPacket();
void     receivePackets();
bool     parsePacket(const String& raw, VehicleData& out);
int      getPeerIndex(uint8_t id);

// GPS helpers
void     feedGPS();
float    calculateSpeed();

// Alert helpers
void     evaluateProximityAlerts();
double   haversineDistance(double lat1, double lon1,
                           double lat2, double lon2);

// Alert primitives
void     triggerDanger();
void     triggerWarning();
void     clearAlerts();
void     blinkLED(int period_ms, int duration_ms);

// Debug print
void     printOwnStatus();
void     printPeerStatus(const VehicleData& v);

// ─────────────────────────────────────────────────────────────────────────────
//  SETUP — runs on Core 1
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(200);

    printStartupBanner();

    //  Initialise all peripherals and report status
    initHardware();

    //  Create shared mutex before spawning tasks
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == nullptr) {
        Serial.println("[FATAL] Could not create mutex. Halting.");
        while (true) { delay(1000); }
    }

    //  Initialise peer table
    for (int i = 0; i < NUM_OTHER_VEHICLES; i++) {
        peers[i].valid = false;
    }

    // ── Assign peer IDs based on VEHICLE_ID ──────────────────────────────
    //  Vehicle 1  → peers: 2, 3
    //  Vehicle 2  → peers: 1, 3
    //  Vehicle 3  → peers: 1, 2
    int idx = 0;
    for (int id = 1; id <= 3; id++) {
        if (id != VEHICLE_ID) {
            peers[idx].id = id;
            idx++;
        }
    }

    Serial.printf("\n[SYSTEM] Vehicle %d initialised. Spawning tasks...\n\n",
                  VEHICLE_ID);

    // ── Spawn FreeRTOS tasks ──────────────────────────────────────────────
    //  Core 0: LoRa communication
    xTaskCreatePinnedToCore(
        taskLoRaComm,           // Task function
        "LoRaComm",             // Name (for debugging)
        8192,                   // Stack size (bytes)
        nullptr,                // Parameters
        2,                      // Priority (higher = more urgent)
        nullptr,                // Task handle
        0                       // Pin to Core 0
    );

    //  Core 1: GPS + alerts
    xTaskCreatePinnedToCore(
        taskGPSAndAlerts,
        "GPSAlerts",
        8192,
        nullptr,
        2,
        nullptr,
        1                       // Pin to Core 1
    );

    // setup() itself runs on Core 1 — we let it return normally.
    // FreeRTOS takes over scheduling.
}

// ─────────────────────────────────────────────────────────────────────────────
//  LOOP — kept empty; all work done in FreeRTOS tasks
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    vTaskDelay(portMAX_DELAY);  // Suspend Arduino loop task
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  TASK: taskLoRaComm  (Core 0)
//  Responsibility: Transmit own packet periodically + receive peer packets.
//
// ═══════════════════════════════════════════════════════════════════════════════
void taskLoRaComm(void* pvParams) {
    TickType_t   lastTxTime  = xTaskGetTickCount();
    const TickType_t txPeriod = pdMS_TO_TICKS(TX_INTERVAL_MS);

    Serial.printf("[Core 0] LoRa task started. TX every %d ms.\n", TX_INTERVAL_MS);

    for (;;) {
        //  ── 1. Receive any incoming packets (non-blocking) ───────────────
        receivePackets();

        //  ── 2. Transmit own data at fixed interval ───────────────────────
        if ((xTaskGetTickCount() - lastTxTime) >= txPeriod) {
            sendOwnPacket();
            lastTxTime = xTaskGetTickCount();
        }

        //  Brief yield to allow scheduler to run other tasks on Core 0
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  TASK: taskGPSAndAlerts  (Core 1)
//  Responsibility: Poll GPS, update own position/speed, evaluate alerts.
//
// ═══════════════════════════════════════════════════════════════════════════════
void taskGPSAndAlerts(void* pvParams) {
    Serial.printf("[Core 1] GPS + Alert task started.\n");

    TickType_t lastPrint = xTaskGetTickCount();

    for (;;) {
        //  ── 1. Feed GPS characters into TinyGPSPlus ──────────────────────
        feedGPS();

        //  ── 2. Update shared ownData if GPS has a new fix ─────────────────
        if (gps.location.isUpdated() && gps.location.isValid()) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
                ownData.latitude  = gps.location.lat();
                ownData.longitude = gps.location.lng();
                ownData.speed     = gps.speed.kmph();
                ownData.gpsFixed  = true;
            xSemaphoreGive(dataMutex);
        }

        //  ── 3. Evaluate proximity and trigger alerts ──────────────────────
        evaluateProximityAlerts();

        //  ── 4. Print status to Serial Monitor every 2 seconds ────────────
        if ((xTaskGetTickCount() - lastPrint) >= pdMS_TO_TICKS(2000)) {
            printOwnStatus();
            for (int i = 0; i < NUM_OTHER_VEHICLES; i++) {
                if (peers[i].valid) printPeerStatus(peers[i]);
            }
            lastPrint = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(GPS_UPDATE_MS));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  HARDWARE INITIALISATION
//
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Calls all init sub-functions, prints status, halts on fatal error.
 */
void initHardware() {
    initAlertPins();

    bool loraOK = initLoRa();
    bool gpsOK  = initGPS();

    printHardwareStatus(loraOK, gpsOK);

    if (!loraOK) {
        Serial.println("[FATAL] LoRa module not found. Check wiring. Halting.");
        while (true) {
            //  Rapid triple-blink to signal LoRa fault
            for (int i = 0; i < 3; i++) {
                digitalWrite(PIN_LED, HIGH); delay(100);
                digitalWrite(PIN_LED, LOW);  delay(100);
            }
            delay(600);
        }
    }

    //  GPS not fatal — vehicle can still relay warnings without its own fix
    if (!gpsOK) {
        Serial.println("[WARN] GPS UART not responding. Proceeding without own GPS.");
    }
}

/**
 * @brief  Initialise LoRa radio with project settings.
 * @return true if LoRa chip found and configured successfully.
 */
bool initLoRa() {
    Serial.print("[INIT] Initialising LoRa... ");

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("FAILED.");
        return false;
    }

    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREAD_FACTOR);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.enableCrc();           // Drop corrupted packets automatically

    Serial.printf("OK  [%.0f MHz, SF%d, BW%.0fkHz, CR4/%d]\n",
                  (double)LORA_FREQUENCY / 1e6,
                  LORA_SPREAD_FACTOR,
                  (double)LORA_BANDWIDTH / 1e3,
                  LORA_CODING_RATE);
    return true;
}

/**
 * @brief  Initialise GPS UART2 and wait briefly for first characters.
 * @return true if GPS appears to be transmitting.
 */
bool initGPS() {
    Serial.print("[INIT] Initialising GPS UART2... ");

    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(500);

    // Check if GPS is sending characters
    unsigned long start = millis();
    bool gotChar = false;
    while (millis() - start < 1500) {
        if (gpsSerial.available()) {
            gotChar = true;
            break;
        }
        delay(10);
    }

    if (gotChar) {
        Serial.printf("OK  [UART2 RX=GPIO%d, %d baud]\n", GPS_RX_PIN, GPS_BAUD);
    } else {
        Serial.println("NO DATA (check wiring / module power).");
    }
    return gotChar;
}

/**
 * @brief  Configure alert GPIO pins.
 */
void initAlertPins() {
    Serial.print("[INIT] Configuring alert pins... ");
    pinMode(PIN_LED,    OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_LED,    LOW);
    digitalWrite(PIN_BUZZER, LOW);

    //  Startup confirmation blink + beep
    for (int i = 0; i < 2; i++) {
        digitalWrite(PIN_LED,    HIGH);
        digitalWrite(PIN_BUZZER, HIGH);
        delay(80);
        digitalWrite(PIN_LED,    LOW);
        digitalWrite(PIN_BUZZER, LOW);
        delay(120);
    }
    Serial.printf("OK  [LED=GPIO%d, Buzzer=GPIO%d]\n", PIN_LED, PIN_BUZZER);
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  LoRa COMMUNICATION
//
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Build and transmit this vehicle's telemetry packet.
 *
 *  Packet format (all values comma-separated, newline-terminated):
 *      <VID>,<LAT>,<LON>,<SPEED_KMH>,<MILLIS_TIMESTAMP>
 *
 *  Example:
 *      "1,12.971599,77.594563,45.30,98765432\n"
 *
 *  FIX: Now broadcasts even without GPS fix (using 0.0 coordinates)
 *       Set ENABLE_SIMULATION_MODE=true to use hardcoded test data
 */
void sendOwnPacket() {
    OwnData snap;

    // Safely snapshot own data
    xSemaphoreTake(dataMutex, portMAX_DELAY);
        snap = ownData;
    xSemaphoreGive(dataMutex);

    // FIX: Use simulated data if enabled (for testing without GPS)
    if (ENABLE_SIMULATION_MODE) {
        snap.latitude = SIM_LATITUDE;
        snap.longitude = SIM_LONGITUDE;
        snap.speed = SIM_SPEED;
        snap.gpsFixed = true;
    }

    // FIX: Transmit even if GPS not fixed (send 0.0 coordinates)
    // This allows vehicles to relay messages and alerts even without own GPS
    
    // Format packet
    char packet[MAX_PACKET_LEN];
    snprintf(packet, sizeof(packet),
             "%d,%.6f,%.6f,%.2f,%lu",
             VEHICLE_ID,
             snap.latitude,
             snap.longitude,
             snap.speed,
             (unsigned long)millis());

    // Transmit
    LoRa.beginPacket();
    LoRa.print(packet);
    int txResult = LoRa.endPacket();    // Blocking until sent

    if (txResult) {
        Serial.printf("[TX  V%d] %s  %s\n", VEHICLE_ID, packet, 
                      snap.gpsFixed ? "[GPS]" : "[NO GPS]");
    } else {
        Serial.printf("[TX  V%d] FAILED to send packet.\n", VEHICLE_ID);
    }
}

/**
 * @brief  Non-blocking check for received LoRa packets.
 *         Parses and stores data from any recognised peer vehicle.
 */
void receivePackets() {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return;

    // Read packet bytes
    String raw = "";
    while (LoRa.available()) {
        raw += (char)LoRa.read();
    }
    raw.trim();

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    // Parse the packet
    VehicleData parsed;
    if (!parsePacket(raw, parsed)) {
        Serial.printf("[RX  ??] Malformed packet: %s\n", raw.c_str());
        return;
    }

    // Ignore packets from ourselves (shouldn't happen, but guard anyway)
    if (parsed.id == VEHICLE_ID) return;

    // Update peer table
    int idx = getPeerIndex(parsed.id);
    if (idx < 0) {
        Serial.printf("[RX  V%d] Unknown peer ID %d — ignoring.\n",
                      parsed.id, parsed.id);
        return;
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
        peers[idx]           = parsed;
        peers[idx].timestamp = millis();    // Overwrite with local receipt time
        peers[idx].lastRssi  = rssi;        // Store signal strength for close-range detection
        peers[idx].valid     = true;
    xSemaphoreGive(dataMutex);

    Serial.printf("[RX  V%d] lat=%.6f lon=%.6f spd=%.1f km/h  RSSI=%d SNR=%.1f\n",
                  parsed.id, parsed.latitude, parsed.longitude,
                  parsed.speed, rssi, snr);
}

/**
 * @brief  Parse a raw LoRa packet string into a VehicleData struct.
 *
 * @param  raw   Raw string from LoRa.read()
 * @param  out   Output struct (populated on success)
 * @return true on success, false if format is wrong
 */
bool parsePacket(const String& raw, VehicleData& out) {
    // Expected format: "VID,LAT,LON,SPEED,TIMESTAMP"
    int f0 = raw.indexOf(',');
    if (f0 < 0) return false;
    int f1 = raw.indexOf(',', f0 + 1);
    if (f1 < 0) return false;
    int f2 = raw.indexOf(',', f1 + 1);
    if (f2 < 0) return false;
    int f3 = raw.indexOf(',', f2 + 1);
    if (f3 < 0) return false;

    out.id        = (uint8_t) raw.substring(0,      f0).toInt();
    out.latitude  =           raw.substring(f0 + 1, f1).toDouble();
    out.longitude =           raw.substring(f1 + 1, f2).toDouble();
    out.speed     =           raw.substring(f2 + 1, f3).toFloat();
    // Timestamp field (f3+1 to end) stored as local millis() in receivePackets()

    // Basic sanity checks
    if (out.id < 1 || out.id > 3)             return false;
    if (out.latitude  < -90.0  || out.latitude  > 90.0)  return false;
    if (out.longitude < -180.0 || out.longitude > 180.0) return false;

    return true;
}

/**
 * @brief  Return the array index in peers[] for a given vehicle ID.
 * @return index (0 or 1) or -1 if not found
 */
int getPeerIndex(uint8_t id) {
    for (int i = 0; i < NUM_OTHER_VEHICLES; i++) {
        if (peers[i].id == id) return i;
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  GPS HELPERS
//
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Drain all available bytes from GPS UART into TinyGPSPlus parser.
 *         Must be called frequently for accurate location data.
 */
void feedGPS() {
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  PROXIMITY ALERT LOGIC
//
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Check distance to all known peers and activate appropriate alerts.
 *
 *  Priority (highest wins):
 *    DANGER  : any peer within DIST_DANGER_M  → fast LED blink + buzzer
 *    WARNING : any peer within DIST_WARNING_M → slow LED blink
 *    CLEAR   : all peers beyond threshold     → all alerts off
 *
 *  Stale peer data (no update within ALERT_TIMEOUT_MS) is ignored.
 *
 *  FIX: If own GPS not fixed, keep checking anyway (distance calc will use 0.0)
 *       This allows relay vehicles without GPS to still participate
 */
void evaluateProximityAlerts() {
    OwnData snap;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
        snap = ownData;
    xSemaphoreGive(dataMutex);

    // FIX: Proceed even without GPS fix (using 0.0 coordinates)
    // This allows vehicles to relay alerts even without their own position

    bool dangerDetected  = false;
    bool warningDetected = false;
    uint32_t now         = millis();

    for (int i = 0; i < NUM_OTHER_VEHICLES; i++) {
        VehicleData peer;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
            peer = peers[i];
        xSemaphoreGive(dataMutex);

        // Skip if no valid data or data is stale
        if (!peer.valid) continue;
        if ((now - peer.timestamp) > ALERT_TIMEOUT_MS) continue;
        
        // FIX: Still process even if peer has 0.0 coords (could be relay mode)
        // Only skip if we BOTH have 0.0 (meaningless distance)
        if (peer.latitude == 0.0 && peer.longitude == 0.0 && 
            snap.latitude == 0.0 && snap.longitude == 0.0) {
            // Both have no fix — can't determine distance, but trigger warning anyway
            warningDetected = true;
            continue;
        }

        // Skip distance calculation if own GPS has no fix (use simple presence alert)
        if (!snap.gpsFixed) {
            // No own position, but peer has data → indicate warning
            if (peer.latitude != 0.0 || peer.longitude != 0.0) {
                warningDetected = true;
            }
            continue;
        }

        // ==================== CHECK 1: RSSI-BASED CLOSE RANGE ====================
        // If signal is VERY strong (RSSI < -40 dBm), devices are 10-15cm apart
        if (peer.lastRssi < RSSI_DANGER_THRESHOLD) {
            Serial.printf("[RSSI] V%d→V%d = %d dBm (Very Close!) — DANGER\n",
                          VEHICLE_ID, peer.id, peer.lastRssi);
            dangerDetected = true;
            break;  // Highest priority — stop checking
        }

        // ==================== CHECK 2: GPS-BASED DISTANCE ====================
        double dist = haversineDistance(snap.latitude,  snap.longitude,
                                        peer.latitude,  peer.longitude) * 100.0;  // Convert m to cm

        Serial.printf("[DIST] V%d→V%d  =  %.1f cm  %s\n",
                      VEHICLE_ID, peer.id, dist,
                      snap.gpsFixed ? "[GPS OK]" : "[NO GPS]");

        if (dist < DIST_DANGER_M) {
            dangerDetected = true;
            break;                  // Highest priority — no need to check more
        } else if (dist < DIST_WARNING_M) {
            warningDetected = true;
        }
    }

    // Apply alert state
    if (dangerDetected) {
        triggerDanger();
    } else if (warningDetected) {
        triggerWarning();
    } else {
        clearAlerts();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Alert state machine (simple non-blocking blink via millis)
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t  alertState      = 0;    // 0=clear  1=warning  2=danger
static uint32_t lastBlinkToggle = 0;
static bool     ledOn           = false;

/**
 * @brief  Activate DANGER alert: fast LED blink + continuous buzzer.
 */
void triggerDanger() {
    alertState = 2;
    // Continuous buzzer
    digitalWrite(PIN_BUZZER, HIGH);

    // Fast non-blocking blink
    uint32_t now = millis();
    if (now - lastBlinkToggle >= (uint32_t)LED_BLINK_FAST_MS) {
        ledOn = !ledOn;
        digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
        lastBlinkToggle = now;
    }
}

/**
 * @brief  Activate WARNING alert: slow LED blink, no buzzer.
 */
void triggerWarning() {
    alertState = 1;
    digitalWrite(PIN_BUZZER, LOW);      // Buzzer off for warning

    uint32_t now = millis();
    if (now - lastBlinkToggle >= (uint32_t)LED_BLINK_SLOW_MS) {
        ledOn = !ledOn;
        digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
        lastBlinkToggle = now;
    }
}

/**
 * @brief  Clear all alerts.
 */
void clearAlerts() {
    alertState = 0;
    ledOn      = false;
    digitalWrite(PIN_LED,    LOW);
    digitalWrite(PIN_BUZZER, LOW);
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  HAVERSINE DISTANCE CALCULATION
//
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Compute great-circle distance between two GPS coordinates.
 *
 *  Uses the Haversine formula for short-range accuracy.
 *
 * @param  lat1  Latitude  of point 1  (decimal degrees)
 * @param  lon1  Longitude of point 1  (decimal degrees)
 * @param  lat2  Latitude  of point 2  (decimal degrees)
 * @param  lon2  Longitude of point 2  (decimal degrees)
 * @return Distance in metres
 */
double haversineDistance(double lat1, double lon1,
                         double lat2, double lon2) {
    const double R = 6371000.0;     // Earth radius in metres
    const double DEG2RAD = M_PI / 180.0;

    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;

    double a = sin(dLat / 2.0) * sin(dLat / 2.0)
             + cos(lat1 * DEG2RAD) * cos(lat2 * DEG2RAD)
             * sin(dLon / 2.0) * sin(dLon / 2.0);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return R * c;
}

// ═══════════════════════════════════════════════════════════════════════════════
//
//  SERIAL MONITOR OUTPUT
//
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief  Print this vehicle's current state to Serial.
 */
void printOwnStatus() {
    OwnData snap;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
        snap = ownData;
    xSemaphoreGive(dataMutex);

    Serial.println("──────────────────────────────────────────");
    Serial.printf(" [OWN  V%d] ", VEHICLE_ID);
    if (snap.gpsFixed) {
        Serial.printf("lat=%.6f  lon=%.6f  spd=%.1f km/h\n",
                      snap.latitude, snap.longitude, snap.speed);
    } else {
        Serial.println("Waiting for GPS fix...");
    }
}

/**
 * @brief  Print a remote vehicle's last-known state to Serial.
 */
void printPeerStatus(const VehicleData& v) {
    uint32_t age = millis() - v.timestamp;
    Serial.printf(" [PEER V%d] lat=%.6f  lon=%.6f  spd=%.1f km/h  age=%lums\n",
                  v.id, v.latitude, v.longitude, v.speed, (unsigned long)age);
}

/**
 * @brief  Print startup ASCII banner to Serial.
 */
void printStartupBanner() {
    Serial.println();
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║   V2V Communication System  — ESP32/LoRa   ║");
    Serial.println("║   Vehicle-to-Vehicle Safety Network        ║");
    Serial.println("╠════════════════════════════════════════════╣");
    Serial.printf( "║   This node: Vehicle ID  =  %d              ║\n", VEHICLE_ID);
    Serial.printf( "║   LoRa Freq: %.0f MHz                    ║\n",
                   (double)LORA_FREQUENCY / 1e6);
    Serial.printf( "║   Danger threshold : %3d cm (12 m)         ║\n",
                   (int)DIST_DANGER_M);
    Serial.printf( "║   Warning threshold: %3d cm (15 m)         ║\n",
                   (int)DIST_WARNING_M);
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.println();
}

/**
 * @brief  Print hardware verification summary table.
 */
void printHardwareStatus(bool loraOK, bool gpsOK) {
    Serial.println("┌──────────────────────────────────┐");
    Serial.println("│     Hardware Verification Check  │");
    Serial.println("├──────────────────────┬───────────┤");
    Serial.printf( "│  LoRa SX1276         │  %-8s │\n", loraOK ? "ONLINE" : "FAILED");
    Serial.printf( "│  NEO-6M GPS          │  %-8s │\n", gpsOK  ? "ONLINE" : "NO DATA");
    Serial.printf( "│  LED  (GPIO %2d)       │  READY    │\n", PIN_LED);
    Serial.printf( "│  Buzzer (GPIO %2d)     │  READY    │\n", PIN_BUZZER);
    Serial.printf( "│  Serial (UART0)      │  %d baud  │\n", SERIAL_BAUD);
    Serial.println("└──────────────────────┴───────────┘");
    Serial.println();
}
