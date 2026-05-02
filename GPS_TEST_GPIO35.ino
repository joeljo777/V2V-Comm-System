/*
 * ============================================================================
 *  GPS TEST - Alternative GPIO 35 (RX-only pin)
 * ============================================================================
 */

#include <Arduino.h>
#include <HardwareSerial.h>

#define GPS_RX_PIN          35      // Try GPIO 35 instead
#define GPS_BAUD            9600

HardwareSerial gpsSerial(1);  // UART1

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n╔═══════════════════════════════════════════╗");
    Serial.println("║  GPS TEST - GPIO 35 (Alternative Pin)    ║");
    Serial.println("║  Wiring: GPS TX → GPIO 35 (no voltage    ║");
    Serial.println("║          divider needed, pin is 3.3V)     ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");
    
    Serial.printf("Initializing GPS on GPIO %d...\n", GPS_RX_PIN);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, -1);  // RX only
    delay(500);
    
    Serial.println("Listening for 30 seconds...\n");
    
    unsigned long startTime = millis();
    int charCount = 0;
    int lineCount = 0;
    
    while (millis() - startTime < 30000) {
        if (gpsSerial.available()) {
            char c = gpsSerial.read();
            charCount++;
            
            if (c >= 32 && c <= 126) {
                Serial.write(c);
            } else if (c == '\n') {
                Serial.println();
                lineCount++;
            }
        }
    }
    
    Serial.printf("\n\n═══ RESULTS ═══\n");
    Serial.printf("Received: %d characters, %d lines\n", charCount, lineCount);
    
    if (charCount > 100 && lineCount > 0) {
        Serial.println("✓ GPS WORKING on GPIO 35!");
        Serial.println("  Update all three files:");
        Serial.println("  #define GPS_RX_PIN  35");
    } else if (charCount > 0) {
        Serial.println("⚠ Receiving some data but corrupted");
        Serial.println("  Try adding voltage divider on GPS TX line");
    } else {
        Serial.println("❌ Still no data - GPS module may be defective");
        Serial.println("   Try a different GPS module or ESP32 board");
    }
}

void loop() {
    delay(1000);
}
