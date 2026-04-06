#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Screen setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pins
#define SS 5
#define RST 14
#define DIO0 2
#define BUZZER 15
#define LED 4

TinyGPSPlus gps;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED Fail");
  
  // --- STARTUP TEST (2 Beeps/Flashes) ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.println("V2V SYSTEM READY");
  display.display();

  for(int i=0; i<2; i++) {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, LOW);
    delay(200);
  }

  // Initialize LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    display.clearDisplay();
    display.println("LORA ERROR!");
    display.display();
    while(1);
  }
}

void loop() {
  // Update local GPS
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // Check for incoming LoRa data from Transmitter
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) { incoming += (char)LoRa.read(); }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, incoming);

    if (!error) {
      checkProximity(doc["lat"], doc["lon"]);
    }
  }
}

void checkProximity(float tLat, float tLon) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("V2V MONITORING...");

  if (!gps.location.isValid()) {
    display.println("\nNO GPS FIX");
    display.println("Check Sky View");
    display.display();
    return;
  }

  // Calculate Haversine distance
  double dist = TinyGPSPlus::distanceBetween(
    gps.location.lat(), gps.location.lng(), 
    tLat, tLon
  );

  display.print("\nDIST: "); display.print(dist, 1); display.println(" m");

  // --- REDUCED DETECTION DISTANCE ---
  // Trigger if closer than 3.0 meters
  if (dist < 3.0) { 
    display.setCursor(0, 40);
    display.setTextSize(2);
    display.println("!! DANGER !!");
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED, HIGH);
  } else {
    display.setCursor(0, 45);
    display.setTextSize(1);
    display.println("STATUS: SAFE");
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED, LOW);
  }
  display.display();
}