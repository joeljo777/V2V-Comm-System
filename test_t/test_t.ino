#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SS 5
#define RST 14
#define DIO0 2
#define BUZZER 15
#define LED 4
#define MY_ID 2

TinyGPSPlus gps;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED Fail");
  
  // --- STARTUP TEST ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.println("V2V SYSTEM START");
  display.display();

  for(int i=0; i<2; i++) {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED, HIGH);
    delay(250);
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, LOW);
    delay(250);
  }

  LoRa.setPins(SS, RST, DIO0);
  LoRa.begin(433E6);
}

void loop() {
  // Update own GPS
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // Check LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) { incoming += (char)LoRa.read(); }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, incoming);

    if (!error && doc["id"] != MY_ID) {
      handleAlertLogic(doc["lat"], doc["lon"]);
    }
  }
}

void handleAlertLogic(float tLat, float tLon) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("V2V MONITOR");

  if (!gps.location.isValid()) {
    display.println("\nSEARCHING FOR GPS...");
    display.display();
    return;
  }

  double dist = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), tLat, tLon);

  display.print("\nDIST: "); display.print(dist, 1); display.println(" m");

  if (dist < 10.0) { // Presentation Distance
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.println("!!!ALERT!!!");
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED, HIGH);
  } else {
    display.println("\nSTATUS: SAFE");
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, LOW);
  }
  display.display();
}