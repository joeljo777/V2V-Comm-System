#include <SPI.h>
#include <LoRa.h>

#define SS 5
#define RST 14
#define DIO0 2

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Initializing...");

  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa INIT FAILED");
    while (1);
  }

  Serial.println("✅ LoRa INIT SUCCESS");
}

void loop() {
  Serial.println("Sending packet...");

  LoRa.beginPacket();
  LoRa.print("Hello JJ");
  LoRa.endPacket();
  Serial.println("Packet sent!");

  delay(2000);
}