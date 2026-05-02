/*
 * ============================================================================
 *  GPS BAUD RATE FINDER
 *  Tests multiple baud rates to find the correct one for your GPS module
 * ============================================================================
 *  Upload this and check Serial Monitor (115200 baud)
 *  The sketch will try: 4800, 9600, 19200, 38400, 57600, 115200
 * ============================================================================
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

#define GPS_RX_PIN          
#define GPS_TX_PIN          27

HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// Baud rates to test
const unsigned long baudRates[] = {4800, 9600, 19200, 38400, 57600, 115200};
const int numBauds = 6;

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n╔═══════════════════════════════════════════╗");
    Serial.println("║     GPS BAUD RATE FINDER                  ║");
    Serial.println("║  Testing: 4800, 9600, 19200, 38400...     ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");
    
    testAllBaudRates();
    
    Serial.println("\n✓ TEST COMPLETE");
    Serial.println("Once you find the correct baud rate, update line 87 in v2v_*.ino:");
    Serial.println("  #define GPS_BAUD  [correct_baud_rate]");
}

void loop() {
    // Nothing here - test runs in setup()
    delay(1000);
}

// ─────────────────────────────────────────────────────────────────────────────
void testAllBaudRates() {
    for (int i = 0; i < numBauds; i++) {
        unsigned long baud = baudRates[i];
        Serial.printf("\n[%d/%d] Testing %lu baud... ", i+1, numBauds, baud);
        
        gpsSerial.end();  // Close previous connection
        delay(100);
        
        gpsSerial.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
        delay(500);
        
        // Try to receive data for 5 seconds
        unsigned long startTime = millis();
        int charCount = 0;
        int validSentences = 0;
        String firstSentence = "";
        
        while (millis() - startTime < 5000) {
            if (gpsSerial.available()) {
                char c = gpsSerial.read();
                charCount++;
                
                // Show first sentence
                if (firstSentence.length() < 100 && c != '\n') {
                    firstSentence += c;
                }
                
                // Feed to TinyGPS
                if (gps.encode(c)) {
                    validSentences++;
                }
            }
        }
        
        Serial.printf("Received %d chars, %d valid sentences\n", charCount, validSentences);
        
        if (charCount > 0) {
            Serial.printf("   First data: %s...\n", firstSentence.c_str());
        }
        
        // Check if we got valid data
        if (validSentences > 0) {
            Serial.printf("\n   ✓✓✓ FOUND IT! Use 9600 baud: #define GPS_BAUD  %lu\n", baud);
            
            // Try to get location
            if (gps.location.isValid()) {
                Serial.printf("   GPS FIX: Lat=%.6f, Lon=%.6f (Sats=%d)\n",
                             gps.location.lat(), gps.location.lng(), 
                             gps.satellites.value());
            }
            return;
        }
    }
    
    Serial.println("\n❌ No valid GPS data found at any baud rate");
    Serial.println("   Troubleshooting:");
    Serial.println("   1. Check GPS TX → ESP32 GPIO 26 (RX) connection");
    Serial.println("   2. Check GPS GND → ESP32 GND connection");
    Serial.println("   3. Check GPS power (5V or 3.3V) - is LED blinking?");
    Serial.println("   4. Try swapping TX/RX pins");
}
