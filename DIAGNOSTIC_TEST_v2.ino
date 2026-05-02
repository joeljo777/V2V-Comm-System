/*
 * ============================================================================
 *  V2V DIAGNOSTIC TEST (Updated for GPIO 9/10)
 *  Use this to verify GPS works on new UART1 pins
 * ============================================================================
 *  Upload to ONE ESP32 board and check Serial Monitor (115200 baud)
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// Pin definitions (UPDATED: GPIO 9/10 for GPS)
#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23
#define LORA_SS             5
#define LORA_RST            14
#define LORA_DIO0           2
#define LORA_FREQUENCY      433E6

#define GPS_RX_PIN          9       // NEW: UART1 RX
#define GPS_TX_PIN          10      // NEW: UART1 TX
#define GPS_BAUD            9600

#define PIN_LED             4
#define PIN_BUZZER          15

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);  // UART1

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    
    printBanner();
    
    Serial.println("\n[1/3] Testing LoRa Module...");
    testLoRa();
    
    Serial.println("\n[2/3] Testing GPS Module (GPIO 9/10)...");
    testGPS();
    
    Serial.println("\n[3/3] Testing Alert Pins...");
    testAlerts();
    
    Serial.println("\n✓ DIAGNOSTIC COMPLETE. See results above.");
}

void loop() {
    // If GPS was detected, print live data
    if (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
        
        if (gps.location.isUpdated()) {
            Serial.printf("[GPS] FIX ACQUIRED! Lat=%.6f Lon=%.6f Sats=%d\n",
                         gps.location.lat(), gps.location.lng(), gps.satellites.value());
        }
    }
    delay(100);
}

// ─────────────────────────────────────────────────────────────────────────────
void testLoRa() {
    Serial.print("  → Initializing SPI bus... ");
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    Serial.println("OK");
    
    Serial.print("  → Setting LoRa pins... ");
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    Serial.println("OK");
    
    Serial.print("  → Attempting LoRa.begin()... ");
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("❌ FAILED");
        Serial.println("     → LoRa module NOT detected on SPI bus");
        Serial.println("     → CHECK: Power (3.3V), RST pin (GPIO 14), SPI connections");
        return;
    }
    Serial.println("✓ SUCCESS");
    
    Serial.println("  → LoRa Configuration:");
    LoRa.setTxPower(17);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setSpreadingFactor(7);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();
    Serial.printf("     - Frequency: %.0f MHz\n", LORA_FREQUENCY / 1e6);
    Serial.println("     - TX Power: 17 dBm, SF7, BW125kHz");
    
    Serial.println("  → Attempting TX test packet... ");
    LoRa.beginPacket();
    LoRa.print("TEST123");
    if (LoRa.endPacket()) {
        Serial.println("✓ TX packet sent successfully");
    } else {
        Serial.println("❌ TX failed (but module may still work)");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void testGPS() {
    Serial.printf("  → Initializing GPS on UART1 (RX=GPIO%d, TX=GPIO%d)... ", 
                  GPS_RX_PIN, GPS_TX_PIN);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("OK");
    
    Serial.print("  → Waiting for GPS data... ");
    unsigned long start = millis();
    bool gotData = false;
    int charCount = 0;
    
    while (millis() - start < 3000) {  // Wait 3 seconds
        if (gpsSerial.available()) {
            char c = gpsSerial.read();
            gps.encode(c);
            charCount++;
            gotData = true;
        }
    }
    
    if (!gotData) {
        Serial.println("❌ NO DATA RECEIVED");
        Serial.println("     → GPS module not transmitting on GPIO 9 (RX)");
        Serial.println("     → CHECK: GPS TX pin → GPIO 9 of ESP32");
        Serial.println("     → CHECK: GPS GND → ESP32 GND");
        Serial.println("     → CHECK: GPS power (5V from VIN pin)");
        Serial.println("     → ADD: Voltage divider (10k+10k resistors) on GPS TX line");
        return;
    }
    
    Serial.printf("✓ Receiving GPS data (%d characters)\n", charCount);
    
    // Check if valid fix
    if (gps.location.isValid()) {
        Serial.printf("  ✓ FIX ACQUIRED: Lat=%.6f Lon=%.6f (Sats=%d)\n",
                     gps.location.lat(), gps.location.lng(), gps.satellites.value());
    } else {
        Serial.println("  ⚠ No GPS fix yet (normal if indoors)");
        Serial.println("     → Move to open sky or wait 2-3 minutes");
        Serial.printf("     → Received %d valid NMEA sentences\n", charCount);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void testAlerts() {
    Serial.print("  → Testing LED (GPIO 4)... ");
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    delay(200);
    digitalWrite(PIN_LED, LOW);
    Serial.println("✓ LED blinked");
    
    Serial.print("  → Testing Buzzer (GPIO 15)... ");
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, HIGH);
    delay(200);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("✓ Buzzer pulsed");
}

// ─────────────────────────────────────────────────────────────────────────────
void printBanner() {
    Serial.println();
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║     V2V DIAGNOSTIC TEST SKETCH (v2)       ║");
    Serial.println("║     GPS on UART1 (GPIO 9/10)              ║");
    Serial.println("╚═══════════════════════════════════════════╝");
}
