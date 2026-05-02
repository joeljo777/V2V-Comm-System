    /*
    * ============================================================================
    *  GPS PIN CONNECTION TESTER
    *  Tries both pin configurations to find correct wiring
    * ============================================================================
    */

    #include <Arduino.h>
    #include <HardwareSerial.h>
    #include <TinyGPSPlus.h>

    HardwareSerial gpsSerial1(2);
    HardwareSerial gpsSerial2(1);  // Alternative UART
    TinyGPSPlus gps;

    void setup() {
        Serial.begin(115200);
        delay(500);
        
        Serial.println("\n╔═══════════════════════════════════════════╗");
        Serial.println("║  GPS PIN CONNECTION TESTER                ║");
        Serial.println("║  Testing: RX=26/TX=27 vs RX=27/TX=26      ║");
        Serial.println("╚═══════════════════════════════════════════╝\n");
        
        // Test configuration 1: RX=26, TX=27 (original)
        Serial.println("[1/2] Testing RX=GPIO26, TX=GPIO27...");
        testConfiguration(26, 27);
        
        // Test configuration 2: Swapped
        Serial.println("\n[2/2] Testing RX=GPIO27, TX=GPIO26...");
        testConfiguration(27, 26);
        
        Serial.println("\n✓ TEST COMPLETE");
    }

    void loop() {
        delay(1000);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    void testConfiguration(int rxPin, int txPin) {
        gpsSerial1.end();
        delay(200);
        
        Serial.printf("  → Initializing UART2 with RX=%d, TX=%d\n", rxPin, txPin);
        gpsSerial1.begin(9600, SERIAL_8N1, rxPin, txPin);
        delay(500);
        
        unsigned long startTime = millis();
        int charCount = 0;
        int validSentences = 0;
        String firstChars = "";
        
        while (millis() - startTime < 3000) {
            if (gpsSerial1.available()) {
                char c = gpsSerial1.read();
                charCount++;
                
                // Collect first 50 characters
                if (firstChars.length() < 50) {
                    if (c >= 32 && c <= 126) {
                        firstChars += c;
                    } else {
                        firstChars += "[" + String(c) + "]";
                    }
                }
                
                // Feed to TinyGPS
                if (gps.encode(c)) {
                    validSentences++;
                }
            }
        }
        
        Serial.printf("  ✓ Received: %d chars, %d valid sentences\n", charCount, validSentences);
        
        if (charCount > 0) {
            Serial.printf("  First data: %s\n", firstChars.c_str());
        }
        
        if (validSentences > 0) {
            Serial.printf("  ✓✓✓ WORKING! Use: RX_PIN=%d, TX_PIN=%d\n", rxPin, txPin);
            
            if (gps.location.isValid()) {
                Serial.printf("  GPS Location: %.6f, %.6f\n", 
                            gps.location.lat(), gps.location.lng());
            } else {
                Serial.println("  No fix yet (normal if indoors)");
            }
        } else if (charCount > 10) {
            Serial.printf("  ⚠ Getting data but corrupted (possibly voltage mismatch)\n");
            Serial.printf("  → Try: Power GPS from 5V instead of 3.3V\n");
            Serial.printf("  → Or: Add voltage divider (10k+10k resistors) on GPS TX line\n");
        }
    }
