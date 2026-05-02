#include <SPI.h>
#include <LoRa.h>

// LoRa pins
#define SS 5
#define RST 14
#define DIO0 2

void setup() {
  Serial.begin(115200);

  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa INIT FAILED");
    while (1);
  }

  Serial.println("✅ Receiver Ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    String received = "";

    while (LoRa.available()) {
      received += (char)LoRa.read();
    }

    Serial.print("📡 ");
    Serial.println(received);
  }
}