#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= WIFI =================
const char* ssid = "ROLEX";
const char* password = "NALIN1710";
String apiKey = "LIKS7JDN6K5FSZGQ";

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= SENSORS =================
Adafruit_BME680 bme;
#define DUST_LED 4
#define DUST_OUT 1  // GPIO1 ADC1_CH0 - PROVEN WORKING

// ================= RELAYS =================
#define RELAY_VENT 16
#define RELAY_PURIFIER 17

// ================= TIMING =================
unsigned long lastDisplayUpdate = 0;
unsigned long lastUpload = 0;
const unsigned long DISPLAY_INTERVAL = 2000;
const unsigned long UPLOAD_INTERVAL = 20000;  // ThingSpeak 15s min

// ================= STATE =================
bool ventState = false, puriState = false;

void setup() {
  Serial.begin(115200);
  
  // Pins first
  pinMode(DUST_LED, OUTPUT);
  pinMode(RELAY_VENT, OUTPUT);
  pinMode(RELAY_PURIFIER, OUTPUT);
  digitalWrite(RELAY_VENT, LOW);
  digitalWrite(RELAY_PURIFIER, LOW);
  
  // I2C - YOUR PROVEN CONFIG
  Wire.begin(8, 9);
  Wire.setClock(100000);
  
  // OLED FIRST - YOUR PROVEN ORDER
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed");
    while(1);
  }
  display.clearDisplay();
  display.display();
  Serial.println("OLED OK");

  // BME680 - YOUR PROVEN CONFIG
  if (!bme.begin()) {
    Serial.println("BME680 not found");
    while(1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_1X);
  bme.setGasHeater(320, 150);
  Serial.println("BME680 OK");

  // WiFi with timeout
  WiFi.begin(ssid, password);
  Serial.print("WiFi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed - offline mode");
  }

  Serial.println("=== AIR QUALITY MONITOR READY ===");
  
  // Boot screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Air Quality Monitor");
  display.println("All Sensors OK");
  display.display();
  delay(1500);
}

float readDust() {
  // YOUR PROVEN TIMING
  digitalWrite(DUST_LED, LOW);
  delayMicroseconds(280);
  int dustRaw = analogRead(DUST_OUT);
  delayMicroseconds(40);
  digitalWrite(DUST_LED, HIGH);
  delayMicroseconds(9680);
  return (dustRaw * 3.3 / 4095.0) * 170.0;
}

void controlDevices(float dust, float voc) {
  bool newVent = ventState;
  bool newPuri = puriState;
  
  // CPCB PM2.5 + VOC thresholds
  if (dust > 120 || voc < 10) {
    newVent = true; newPuri = true;
  } else if (dust > 90 || voc < 30) {
    newVent = false; newPuri = true;
  } else if (dust > 60 || voc < 50) {
    newVent = true; newPuri = false;
  } else {
    newVent = false; newPuri = false;
  }
  
  if (newVent != ventState) {
    digitalWrite(RELAY_VENT, newVent);
    ventState = newVent;
    Serial.println(ventState ? "VENT ON" : "VENT OFF");
  }
  if (newPuri != puriState) {
    digitalWrite(RELAY_PURIFIER, newPuri);
    puriState = newPuri;
    Serial.println(puriState ? "PURIFIER ON" : "PURIFIER OFF");
  }
}

void updateDisplay(float temp, float hum, float pressure, float voc, float dust) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.print("T:"); display.print(temp, 1); display.println("C");
  display.print("H:"); display.print(hum, 0); display.println("%");
  display.print("P:"); display.print(pressure, 0); display.println("hPa");
  
  display.print("VOC:");
  if (voc > 999) display.print(voc/1000.0, 1);
  else display.print(voc, 0);
  display.println("k");
  
  display.print("Dust:"); display.print(dust, 0); display.println("ug/m3");
  
  // Status
  display.print("V:"); display.print(ventState ? "ON " : "OFF");
  display.print(" P:"); display.print(puriState ? "ON" : "OFF");
  
  display.display();
  delay(50);  // I2C stabilize
}

void uploadToThingSpeak(float temp, float hum, float pressure, float voc, float dust) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.setTimeout(5000);
  
  String url = "http://api.thingspeak.com/update?api_key=" + apiKey +
               "&field1=" + String(temp,1) +
               "&field2=" + String(hum,0) +
               "&field3=" + String(pressure,0) +
               "&field4=" + String(voc,0) +
               "&field5=" + String(dust,0) +
               "&field6=" + String(ventState) +
               "&field7=" + String(puriState);
  
  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    Serial.println("ThingSpeak OK");
  } else {
    Serial.println("ThingSpeak fail: " + String(code));
  }
  http.end();
}

void loop() {
  unsigned long now = millis();
  
  // Sensor read - YOUR PROVEN SEQUENCE
  if (!bme.performReading()) {
    Serial.println("BME read failed");
    delay(1000);
    return;
  }
  
  float temp = bme.temperature;
  float hum = bme.humidity;
  float pressure = bme.pressure / 100.0;
  float voc = bme.gas_resistance / 1000.0;
  float dust = readDust();
  
  // Control
  controlDevices(dust, voc);
  
  // Serial - YOUR PROVEN FORMAT
  Serial.printf("T:%.1f H:%.0f P:%.0f VOC:%.0f Dust:%.0f\n\r", 
                temp, hum, pressure, voc, dust);
  
  // Display - YOUR PROVEN TIMING
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay(temp, hum, pressure, voc, dust);
    lastDisplayUpdate = now;
  }
  
  // ThingSpeak upload
  if (now - lastUpload >= UPLOAD_INTERVAL) {
    uploadToThingSpeak(temp, hum, pressure, voc, dust);
    lastUpload = now;
  }
  
  delay(80);  // I2C recovery - YOUR PROVEN
}
