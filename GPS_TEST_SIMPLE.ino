/*
 * ============================================================================
 *  SIMPLE GPS TEST FOR GPIO 26/27
 *  Just shows raw GPS data being received
 * ============================================================================
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

#define GPS_RX_PIN          26      // Your actual hardware
#define GPS_TX_PIN          27
#define GPS_BAUD            9600

HardwareSerial gpsSerial(2);  // UART20 asqas
TinyGPSPlus gps;

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n╔═══════════════════════════════════════════╗");
    Serial.println("║  GPS TEST - GPIO 26/27                   ║");
    Serial.println("║  Hardware: NEO-6M on UART2               ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");
    
    Serial.printf("Starting GPS on RX=GPIO%d, TX=GPIO%d\n", GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("Listening for 10 seconds...\n");
    
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(500);
    
    unsigned long startTime = millis();
    int charCount = 0;
    int validCount = 0;
    
    while (millis() - startTime < 10000) {
        if (gpsSerial.available()) {
            char c = gpsSerial.read();
            charCount++;
            
            if (gps.encode(c)) {
                validCount++;
            }
            
            // Print readable characters
            if (c >= 32 && c <= 126) {
                Serial.write(c);
            }
            
            // Line break on newline
            if (c == '\n') {
                Serial.println();
            }
        }
    }
    
    Serial.printf("\n\nResults:\n");
    Serial.printf("  Total characters: %d\n", charCount);
    Serial.printf("  Valid sentences: %d\n", validCount);
    
    if (charCount > 0 && validCount == 0) {
        Serial.println("\n⚠️  PROBLEM: Receiving data but can't parse it");
        Serial.println("   → ADD VOLTAGE DIVIDER on GPS TX line:");
        Serial.println("   → GPS TX --|10kΩ|--+--|10kΩ|--GND");
        Serial.println("                      |");
        Serial.println("                  GPIO 26");
    } else if (validCount > 0) {
        Serial.println("\n✓ SUCCESS! GPS working properly");
        if (gps.location.isValid()) {
            Serial.printf("   Lat: %.6f, Lon: %.6f\n", 
                         gps.location.lat(), gps.location.lng());
        }
    } else {
        Serial.println("\n❌ NO DATA: Check wiring!");
    }
}

void loop() {
    delay(1000);
}
