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

// GP2Y Pins
#define DUST_LED 4
#define DUST_OUT 1   // Analog pin
/*
void setup() {

  Serial.begin(115200);
  Wire.begin(8, 9);   // SDA, SCL

  pinMode(DUST_LED, OUTPUT);

  // OLED init
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed");
    while(1);
  }

  // BME680 init
  if (!bme.begin()) {
    Serial.println("BME680 not found");   
    while (1);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setGasHeater(320, 150);

  display.clearDisplay();
}*/

float readDust() {

  digitalWrite(DUST_LED, LOW);
  delayMicroseconds(280);

  int dustValue = analogRead(DUST_OUT);

  delayMicroseconds(40);
  digitalWrite(DUST_LED, HIGH);
  delayMicroseconds(9680);

  return dustValue;
}
/*
void loop() {

  if (!bme.performReading()) return;

  float temp = bme.temperature;
  float hum = bme.humidity;
  float voc = bme.gas_resistance / 1000.0;

  float dust = readDust();

  Serial.println("----- IAQ DATA -----");
  Serial.println(temp);
  Serial.println(hum);
  Serial.println(voc);
  Serial.println(dust);

  // OLED Display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);

  display.print("Temp: ");
  display.print(temp);
  display.println(" C");

  display.print("Hum: ");
  display.print(hum);
  display.println(" %");

  display.print("VOC: ");
  display.print(voc);
  display.println(" KOhm");

  display.print("Dust: ");
  display.println(dust);

  display.display();

  delay(2000);
}

void loop() {
  // Read BME680 FIRST (heavy I2C)
  if (!bme.performReading()) {
    Serial.println("BME read failed");
    delay(1000);
    return;
  }
  
  float temp = bme.temperature;
  float hum = bme.humidity;
  float voc = bme.gas_resistance / 1000.0;
  float dust = readDust();  // Non-I2C

  Serial.print("T:"); Serial.print(temp);
  Serial.print(" H:"); Serial.print(hum);
  Serial.print(" VOC:"); Serial.print(voc);
  Serial.print(" Dust:"); Serial.println(dust);

  // I2C SETTLE TIME - Critical!
  delay(50);
  
  // Display block - short I2C bursts only
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  
  display.print("T:"); display.print(temp,1); display.println("C ");
  display.print("H:"); display.print(hum,1); display.println("% ");
  display.print("VOC:"); display.print(voc,0); display.println("k ");
  display.print("Dust:"); display.println(dust,0);
  
  display.display();
  
  // POST-DISPLAY SETTLE
  delay(100);
  
  delay(2000);  // Main loop delay
}
*/


void loop() {
  // 1. BME680 read (BLOCKING - takes ~150ms)
  if (!bme.performReading()) {
    Serial.println("BME failed");
    return;
  }
  
  float temp = bme.temperature;
  float hum = bme.humidity;
  float voc = bme.gas_resistance / 1000.0;
  float dust = readDust();

  Serial.println("----- IAQ DATA -----");
  Serial.println(temp); Serial.println(hum); 
  Serial.println(voc); Serial.println(dust);

  // 2. CRITICAL: I2C bus recovery
  delay(100);  // Let BME680 transaction complete

  // 3. OLED update (SHORT TRANSACTION)
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  
  display.print("T:"); display.print(temp, 1); display.println("C");
  display.print("H:"); display.print(hum, 1); display.println("%");
  display.print("VOC:"); display.print(voc, 0); display.println("k");
  display.print("Dust:"); display.println((int)dust);
  
  display.display();

  // 4. POST-DISPLAY STABILIZATION
  delay(100);

  delay(2000);
}
