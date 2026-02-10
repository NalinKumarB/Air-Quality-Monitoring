#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// BME680
Adafruit_BME680 bme;

// Dust Sensor
#define DUST_LED 4
#define DUST_OUT 1  // GPIO1 ADC1_CH0

// Timing
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 2000;

void setup() {
  Serial.begin(115200);
  
  Wire.begin(8, 9);
  Wire.setClock(100000);
  
  // OLED FIRST
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed");
    while(1);
  }
  display.clearDisplay();
  display.display();
  Serial.println("OLED OK");

  // BME680
  if (!bme.begin()) {
    Serial.println("BME680 not found");
    while(1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_1X);
  bme.setGasHeater(320, 150);
  Serial.println("BME680 OK");

  pinMode(DUST_LED, OUTPUT);
  Serial.println("=== SYSTEM READY ===");
}

float readDust() {
  digitalWrite(DUST_LED, LOW);
  delayMicroseconds(280);
  
  int dustRaw = analogRead(DUST_OUT);
  
  delayMicroseconds(40);
  digitalWrite(DUST_LED, HIGH);
  delayMicroseconds(9680);
  
  return (dustRaw * 3.3 / 4095.0) * 170.0;
}

void loop() {
  unsigned long now = millis();
  
  // ✅ CRITICAL: TRIGGER MEASUREMENT FIRST
  if (!bme.performReading()) {
    Serial.println("BME read failed");
    delay(1000);
    return;
  }
  
  // ✅ NOW read values (after performReading)
  float temp = bme.temperature;
  float hum = bme.humidity;
  float pressure = bme.pressure / 100.0;
  float voc = bme.gas_resistance / 1000.0;
  float dust = readDust();
  
  Serial.printf("T:%.1fC H:%.1f%% P:%.0fhPa VOC:%.0fk Dust:%.0f\n", 
                temp, hum, pressure, voc, dust);

  delay(80);  // I2C recovery
  
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay(temp, hum, pressure, voc, dust);
    lastDisplayUpdate = now;
  }
  
  delay(100);
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
  
  display.display();
  delay(50);
}


/*#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED - PROVEN WORKING CONFIG
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// BME680
Adafruit_BME680 bme;

// Dust Sensor
#define DUST_LED 4
#define DUST_OUT 1  // GPIO1 (ADC1_CH0) - VALID on ESP32-S3

// Timing for smooth operation
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 2000;

void setup() {
  Serial.begin(115200);
  
  // I2C - EXACTLY like your working test
  Wire.begin(8, 9);  // SDA=8, SCL=9
  Wire.setClock(100000);  // Stable speed
  
  // OLED FIRST (your proven sequence)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed");
    while(1);
  }
  display.clearDisplay();
  display.display();
  Serial.println("OLED OK");

  // BME680 SECOND
  if (!bme.begin()) {
    Serial.println("BME680 not found");
    while(1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_1X);
  bme.setGasHeater(320, 150);  // 320°C for 150ms
  Serial.println("BME680 OK");

  pinMode(DUST_LED, OUTPUT);
  Serial.println("=== SYSTEM READY ===");
}

float readDust() {
  digitalWrite(DUST_LED, LOW);
  delayMicroseconds(280);
  
  int dustRaw = analogRead(DUST_OUT);  // 0-4095 on ESP32-S3
  
  delayMicroseconds(40);
  digitalWrite(DUST_LED, HIGH);
  delayMicroseconds(9680);
  
  // Convert to approximate ug/m³
  return (dustRaw * 3.3 / 4095.0) * 170.0;
}

void loop() {
  unsigned long now = millis();
  
  // Read sensors (non-blocking where possible)
  float temp = bme.temperature;
  float hum = bme.humidity;
  float pressure = bme.pressure / 100.0;  // hPa
  float voc = bme.gas_resistance / 1000.0;  // kOhms
  float dust = readDust();
  
  // Print to Serial
  Serial.printf("T:%.1fC H:%.1f%% P:%.0fhPa VOC:%.0fk Dust:%.0f\n", 
                temp, hum, pressure, voc, dust);

  // CRITICAL: I2C bus recovery after BME680
  delay(80);
  
  // Update display every 2 seconds
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay(temp, hum, pressure, voc, dust);
    lastDisplayUpdate = now;
  }
  
  delay(100);  // Stabilize loop
}

void updateDisplay(float temp, float hum, float pressure, float voc, float dust) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Line 1: Temperature
  display.print("T:"); display.print(temp, 1); display.println("C");
  
  // Line 2: Humidity  
  display.print("H:"); display.print(hum, 0); display.println("%");
  
  // Line 3: Pressure
  display.print("P:"); display.print(pressure, 0); display.println("hPa");
  
  // Line 4: VOC
  display.print("VOC:"); 
  if (voc > 999) display.print(voc/1000.0, 1); 
  else display.print(voc, 0);
  display.println("k");
  
  // Line 5: Dust
  display.print("Dust:"); display.print(dust, 0); display.println("ug/m3");
  
  // Double buffer - CRITICAL for ESP32-S3
  display.display();
  
  // Post-display stabilization
  delay(50);
}*/
