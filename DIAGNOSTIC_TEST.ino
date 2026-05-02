/*
 * ============================================================================
 *  V2V DIAGNOSTIC TEST SKETCH
 *  Use this to identify which component is failing
 * ============================================================================
 *  Upload to ONE ESP32 board and check Serial Monitor (115200 baud)
 *  This tests LoRa, GPS, and connections independently
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// Pin definitions (same as main code)
#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23
#define LORA_SS             5
#define LORA_RST            14
#define LORA_DIO0           2
#define LORA_FREQUENCY      433E6

#define GPS_RX_PIN          26
#define GPS_TX_PIN          27
#define GPS_BAUD            9600

#define PIN_LED             4
#define PIN_BUZZER          15

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    
    printBanner();
    
    Serial.println("\n[1/3] Testing LoRa Module...");
    testLoRa();
    
    Serial.println("\n[2/3] Testing GPS Module...");
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
    Serial.print("  → Initializing GPS UART2... ");
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
        Serial.println("     → GPS module not transmitting on GPIO 26 (RX)");
        Serial.println("     → CHECK: TX pin of GPS module → GPIO 26 of ESP32");
        Serial.println("     → CHECK: GPS module power (5V or 3.3V)");
        Serial.println("     → CHECK: Baud rate (default 9600, but some modules use 4800)");
        return;
    }
    
    Serial.printf("✓ Receiving GPS data (%d characters)\n", charCount);
    
    // Check if valid fix
    if (gps.location.isValid()) {
        Serial.printf("  ✓ FIX ACQUIRED: Lat=%.6f Lon=%.6f (Sats=%d)\n",
                     gps.location.lat(), gps.location.lng(), gps.satellites.value());
    } else {
        Serial.println("  ⚠ No GPS fix yet (normal if indoors)");
        Serial.println("     → Move to open sky or use GPS simulator");
        Serial.printf("     → Received %d characters. Waiting for valid NMEA sentences...\n", charCount);
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
    Serial.println("║     V2V DIAGNOSTIC TEST SKETCH            ║");
    Serial.println("║     Testing: LoRa + GPS + Alerts          ║");
    Serial.println("╚═══════════════════════════════════════════╝");
}
