#include <HardwareSerial.h>
#include <SPI.h>
#include <LoRa.h>

// LoRa pins
#define SS 5
#define RST 14
#define DIO0 2

HardwareSerial gpsSerial(2);

void setup() {
  Serial.begin(115200);

  // GPS UART
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  // LoRa setup
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa INIT FAILED");
    while (1);
  }

  Serial.println("✅ TX Ready (GPS → LoRa)");
}

void loop() {
  if (gpsSerial.available()) {

    // Read full GPS sentence
    String sentence = gpsSerial.readStringUntil('\n');

    // Print locally
    Serial.println(sentence);

    // Send via LoRa
    LoRa.beginPacket();
    LoRa.print(sentence);
    LoRa.endPacket();

    Serial.println("📡 Sent\n");
  }
}