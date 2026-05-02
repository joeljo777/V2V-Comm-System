/*
 * ============================================================================
 *  GPS RAW DATA VIEWER
 *  Shows exactly what bytes are being received from GPS
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
    Serial.println("║     GPS RAW DATA VIEWER                   ║");
    Serial.println("║  Showing: Character codes + ASCII text    ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");
    
    Serial.printf("Opening GPS on RX=%d, TX=%d at %d baud...\n", 
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(500);
    
    Serial.println("Listening for GPS data (30 seconds)...\n");
}

void loop() {
    if (gpsSerial.available()) {
        char c = gpsSerial.read();
        
        // Print as: ASCII (hex) | ASCII char
        if (c >= 32 && c <= 126) {
            // Printable character
            Serial.printf("%c [0x%02X] ", c, c);
        } else {
            // Non-printable character
            Serial.printf("  [0x%02X] ", c);
        }
        
        // New line at line feeds
        if (c == '\n') {
            Serial.println();
        }
    }
}
