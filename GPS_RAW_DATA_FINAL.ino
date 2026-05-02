/*
 * ============================================================================
 *  GPS RAW DATA TEST - Check if GPS is sending VALID NMEA
 * ============================================================================
 */

#include <Arduino.h>
#include <HardwareSerial.h>

#define GPS_RX_PIN          26
#define GPS_TX_PIN          27
#define GPS_BAUD            9600

HardwareSerial gpsSerial(2);

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n╔═══════════════════════════════════════════╗");
    Serial.println("║     GPS RAW DATA - 20 Seconds Test       ║");
    Serial.println("║  Looking for: $GPRMC with valid data     ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");
    
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(500);
    
    unsigned long startTime = millis();
    int validCount = 0;
    
    while (millis() - startTime < 20000) {
        if (gpsSerial.available()) {
            char c = gpsSerial.read();
            
            // Print all characters
            if (c >= 32 && c <= 126) {
                Serial.write(c);
            } else if (c == '\n') {
                Serial.println();
                validCount++;
            } else if (c == '\r') {
                // Skip CR
            } else {
                Serial.printf("[0x%02X]", c);
            }
        }
    }
    
    Serial.printf("\n\nReceived %d lines\n", validCount);
    Serial.println("\nIf you see lines starting with '$GPRMC' → GPS is working");
    Serial.println("If mostly garbage → Still have voltage/connection issue");
}

void loop() {
    delay(1000);
}
