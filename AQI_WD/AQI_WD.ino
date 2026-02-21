#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= WIFI =================
const char* ssid = "ROLEX";
const char* password = "NALIN1710";

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= SENSORS =================
Adafruit_BME680 bme;
#define DUST_LED 4
#define DUST_OUT 1

// ================= RELAYS =================
#define RELAY_VENT 16
#define RELAY_PURIFIER 17

// ================= PUSH BUTTON =================
#define CONTROL_BUTTON 2    // GPIO2 - Safe, onboard LED bonus

// ================= WEB SERVER =================
AsyncWebServer server(80);
AsyncEventSource events("/events");

// ================= TIMING =================
unsigned long lastDisplayUpdate = 0;
unsigned long lastWebUpdate = 0;
unsigned long lastButtonPress = 0;
const unsigned long DISPLAY_INTERVAL = 2000;
const unsigned long WEB_INTERVAL = 3000;
const unsigned long DEBOUNCE_DELAY = 200;

// ================= STATE =================
bool ventState = false, puriState = false;
bool showAQI = false;
bool buttonPressed = false;
bool lastButtonState = true;
int buttonMode = 0;
float temp, hum, pressure, voc, dust;
String aqiStatus = "GOOD";

void setup() {
  Serial.begin(115200);
  
  // Pins
  pinMode(DUST_LED, OUTPUT);
  pinMode(RELAY_VENT, OUTPUT);
  pinMode(RELAY_PURIFIER, OUTPUT);
  pinMode(CONTROL_BUTTON, INPUT_PULLUP);
  digitalWrite(RELAY_VENT, LOW);
  digitalWrite(RELAY_PURIFIER, LOW);
  
  // I2C - YOUR PROVEN CONFIG
  Wire.begin(8, 9);
  Wire.setClock(100000);
  
  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED Failed");
    while(1);
  }
  display.clearDisplay();
  display.display();
  Serial.println("✅ OLED OK");

  // BME680
  if (!bme.begin()) {
    Serial.println("❌ BME680 not found");
    while(1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_1X);
  bme.setGasHeater(320, 150);
  Serial.println("✅ BME680 OK");
  Serial.println("✅ Button OK - GPIO2");

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("📶 WiFi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }
  Serial.println("\n✅ IP: http://" + WiFi.localIP().toString());

  // Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getHTML());
  });
  
  server.on("/vent", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      ventState = (request->getParam("state")->value() == "1");
      digitalWrite(RELAY_VENT, ventState);
      Serial.println("🌐 Web: Vent " + String(ventState ? "ON" : "OFF"));
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/purifier", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      puriState = (request->getParam("state")->value() == "1");
      digitalWrite(RELAY_PURIFIER, puriState);
      Serial.println("🌐 Web: Purifier " + String(puriState ? "ON" : "OFF"));
    }
    request->send(200, "text/plain", "OK");
  });
  
  events.onConnect([](AsyncEventSourceClient *client){
    client->send("init", NULL, millis(), 1000);
  });
  server.addHandler(&events);

  server.begin();
  Serial.println("🚀 PREMIUM SYSTEM READY!");
  Serial.println("📱 Web: http://" + WiFi.localIP().toString());
  Serial.println("🔘 Button: GPIO2 (4 modes)");
  Serial.println("🔌 Vent: GPIO16 | Purifier: GPIO17");
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

String getAQIStatus(float pm25, float voc) {
  if (pm25 > 120 || voc < 10) return "VERY POOR";
  else if (pm25 > 90 || voc < 30) return "POOR";
  else if (pm25 > 60 || voc < 50) return "MODERATE";
  else return "GOOD";
}

void controlDevices(float dust, float voc) {
  bool newVent = ventState;
  bool newPuri = puriState;
  
  if (dust > 120 || voc < 10) {
    newVent = true; newPuri = true;
  } else if (dust > 90 || voc < 30) {
    newPuri = true;
  } else if (dust > 60 || voc < 50) {
    newVent = true;
  } else {
    newVent = false; newPuri = false;
  }
  
  if (newVent != ventState) {
    digitalWrite(RELAY_VENT, newVent);
    ventState = newVent;
    Serial.println("🤖 Auto: Vent " + String(ventState ? "ON" : "OFF"));
  }
  if (newPuri != puriState) {
    digitalWrite(RELAY_PURIFIER, newPuri);
    puriState = newPuri;
    Serial.println("🤖 Auto: Purifier " + String(puriState ? "ON" : "OFF"));
  }
}

void showAQIDisplay(float d, float v, String status) {
  display.clearDisplay();
  
  // BIG AQI
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 8);
  display.print(status);
  
  // Details
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Dust:"); display.print(d, 0); display.print(" ug");
  display.setCursor(64, 35);
  display.print("V:"); display.print(ventState ? "ON" : "OF");
  
  display.setCursor(0, 48);
  display.print("VOC: "); display.print(v, 0); display.print(" kOhm");
  display.setCursor(64, 48);
  display.print("P:"); display.print(puriState ? "ON" : "OF");
  
  display.display();
  delay(50);
}

void updateDisplay(float t, float h, float p, float v, float d) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.print("T:"); display.print(t, 1); display.println("C");
  display.print("H:"); display.print(h, 0); display.println("%");
  display.print("P:"); display.print(p, 0); display.println("hPa");
  display.print("VOC:"); display.print(v, 0); display.println("k");
  display.print("Dust:"); display.print(d, 0); display.println("ug/m3");
  display.print("V:"); display.print(ventState ? "ON " : "OFF");
  display.print(" P:"); display.print(puriState ? "ON" : "OFF");
  display.display();
  delay(50);
}

String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Air Quality Monitor</title>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: 'Inter', sans-serif;
      background: linear-gradient(135deg, #0f0f23 0%, #1a1a2e 50%, #16213e 100%);
      color: #e2e8f0;
      min-height: 100vh;
      padding: 2rem;
      overflow-x: hidden;
    }
    .container { max-width: 1400px; margin: 0 auto; }
    .header { text-align: center; margin-bottom: 4rem; position: relative; }
    .header::before { content: ''; position: absolute; top: -2rem; left: 50%; transform: translateX(-50%); width: 100px; height: 4px; background: linear-gradient(90deg, #00d4ff, #7b68ee); border-radius: 2px; }
    .header h1 { font-size: clamp(2.5rem, 5vw, 4rem); font-weight: 700; background: linear-gradient(135deg, #00d4ff, #7b68ee, #ff6b9d); -webkit-background-clip: text; -webkit-text-fill-color: transparent; margin-bottom: 1rem; letter-spacing: -0.02em; }
    .status-hero { background: rgba(15, 15, 35, 0.4); backdrop-filter: blur(20px); border: 1px solid rgba(120, 119, 198, 0.2); border-radius: 32px; padding: 3rem 2rem; margin-bottom: 3rem; text-align: center; position: relative; overflow: hidden; }
    .status-hero::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 1px; background: linear-gradient(90deg, transparent, rgba(120,119,198,0.3), transparent); }
    .aqi-primary { font-size: clamp(4rem, 10vw, 7rem); font-weight: 800; margin: 1rem 0 0.5rem; line-height: 1; }
    .good { color: #10b981; text-shadow: 0 0 30px rgba(16,185,129,0.5); }
    .moderate { color: #f59e0b; text-shadow: 0 0 30px rgba(245,158,11,0.5); }
    .poor { color: #f97316; text-shadow: 0 0 30px rgba(249,115,22,0.5); }
    .very-poor { color: #ef4444; text-shadow: 0 0 30px rgba(239,68,68,0.5); }
    .status-subtitle { font-size: 1.3rem; opacity: 0.8; margin-top: 1rem; font-weight: 400; }
    .gauges-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 2rem; margin-bottom: 3rem; }
    .gauge-card { background: rgba(20, 20, 40, 0.6); backdrop-filter: blur(16px); border: 1px solid rgba(120, 119, 198, 0.15); border-radius: 24px; padding: 2rem; position: relative; overflow: hidden; transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1); }
    .gauge-card:hover { transform: translateY(-8px); border-color: rgba(120, 119, 198, 0.4); box-shadow: 0 25px 50px -12px rgba(0,0,0,0.5); }
    .gauge-card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 1px; background: linear-gradient(90deg, transparent, #00d4ff, transparent); }
    .gauge-icon { font-size: 2.5rem; margin-bottom: 1rem; opacity: 0.8; }
    .gauge-label { font-size: 0.95rem; text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 1rem; color: #94a3b8; font-weight: 500; }
    .gauge-value { font-size: clamp(2.5rem, 6vw, 4rem); font-weight: 700; margin-bottom: 0.5rem; background: linear-gradient(135deg, #e2e8f0, #cbd5e1); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .gauge-unit { font-size: 1rem; opacity: 0.7; font-weight: 400; }
    .chart-container { background: rgba(20, 20, 40, 0.6); backdrop-filter: blur(16px); border: 1px solid rgba(120, 119, 198, 0.15); border-radius: 24px; padding: 2rem; margin-bottom: 3rem; position: relative; height: 300px; }
    .chart-container::before { content: '📈 Live PM2.5 Trend'; position: absolute; top: 1rem; left: 2rem; font-size: 1.1rem; font-weight: 600; color: #94a3b8; }
    .controls-panel { background: rgba(20, 20, 40, 0.6); backdrop-filter: blur(16px); border: 1px solid rgba(120, 119, 198, 0.15); border-radius: 24px; padding: 2.5rem; text-align: center; }
    .controls-title { font-size: 1.5rem; font-weight: 600; margin-bottom: 2rem; background: linear-gradient(135deg, #00d4ff, #7b68ee); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .control-btn { background: linear-gradient(135deg, #00d4ff, #0099cc); color: white; border: none; padding: 1rem 2.5rem; margin: 0 1rem 1rem 0; border-radius: 50px; font-size: 1.1rem; font-weight: 600; cursor: pointer; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); box-shadow: 0 10px 30px rgba(0,212,255,0.3); position: relative; overflow: hidden; }
    .control-btn::before { content: ''; position: absolute; top: 0; left: -100%; width: 100%; height: 100%; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.3), transparent); transition: left 0.5s; }
    .control-btn:hover::before { left: 100%; }
    .control-btn:hover { transform: translateY(-4px) scale(1.02); box-shadow: 0 20px 40px rgba(0,212,255,0.4); }
    .control-btn.off { background: linear-gradient(135deg, #ef4444, #dc2626); box-shadow: 0 10px 30px rgba(239,68,68,0.3); }
    .control-btn.off:hover { box-shadow: 0 20px 40px rgba(239,68,68,0.4); }
    .status-info { margin-top: 2rem; padding: 1.5rem; background: rgba(16,185,129,0.1); border: 1px solid rgba(16,185,129,0.3); border-radius: 16px; font-size: 0.95rem; opacity: 0.9; }
    @media (max-width: 768px) { body { padding: 1rem; } .gauges-grid { grid-template-columns: 1fr; gap: 1.5rem; } .status-hero { padding: 2rem 1.5rem; } .control-btn { display: block; margin: 0.5rem auto; max-width: 280px; } }
    @keyframes fadeInUp { from { opacity: 0; transform: translateY(30px); } to { opacity: 1; transform: translateY(0); } }
    .fade-in-up { animation: fadeInUp 0.8s cubic-bezier(0.4, 0, 0.2, 1); }
  </style>
</head>
<body>
  <div class="container">
    <div class="header fade-in-up">
      <h1>🌡️ Premium Air Quality Monitor</h1>
      <div id="status-hero" class="status-hero fade-in-up">
        <div class="aqi-primary good" id="aqi-text">GOOD</div>
        <div class="status-subtitle" id="status-subtitle">Optimal air quality detected</div>
      </div>
    </div>
    <div class="gauges-grid fade-in-up" id="gauges-grid">
      <div class="gauge-card"><div class="gauge-icon">🌡️</div><div class="gauge-label">Temperature</div><div class="gauge-value" id="temp">--</div><div class="gauge-unit">°C</div></div>
      <div class="gauge-card"><div class="gauge-icon">💧</div><div class="gauge-label">Humidity</div><div class="gauge-value" id="humidity">--</div><div class="gauge-unit">%</div></div>
      <div class="gauge-card"><div class="gauge-icon">📊</div><div class="gauge-label">Pressure</div><div class="gauge-value" id="pressure">--</div><div class="gauge-unit">hPa</div></div>
      <div class="gauge-card"><div class="gauge-icon">💨</div><div class="gauge-label">VOC</div><div class="gauge-value" id="voc">--</div><div class="gauge-unit">kΩ</div></div>
      <div class="gauge-card"><div class="gauge-icon">☁️</div><div class="gauge-label">PM2.5</div><div class="gauge-value" id="dust">--</div><div class="gauge-unit">μg/m³</div></div>
    </div>
    <div class="chart-container fade-in-up"><canvas id="pm25Chart"></canvas></div>
    <div class="controls-panel fade-in-up">
      <div class="controls-title">🛠️ Smart Device Controls</div>
      <button class="control-btn" id="vent-btn" onclick="toggleVent()">Ventilation: OFF</button>
      <button class="control-btn" id="purifier-btn" onclick="togglePurifier()">Purifier: OFF</button>
      <div class="status-info">📶 <span id="ip-address"></span> | 🔄 Live: 3s | 💾 OLED Active | 🔘 Button: GPIO2</div>
    </div>
  </div>
  <script>
    const ctx = document.getElementById('pm25Chart').getContext('2d');
    const pm25Chart = new Chart(ctx, {type: 'line', data: {labels: [], datasets: [{label: 'PM2.5 (μg/m³)', data: [], borderColor: '#00d4ff', backgroundColor: 'rgba(0,212,255,0.1)', tension: 0.4, fill: true, pointRadius: 0, borderWidth: 3}]}, options: {responsive: true, maintainAspectRatio: false, plugins: {legend: {display: false}}, scales: {x: {display: false}, y: {beginAtZero: true, grid: {color: 'rgba(120,119,198,0.1)'}, ticks: {color: '#94a3b8'}}}}});
    const eventSource = new EventSource('/events');
    document.getElementById('ip-address').innerText = location.hostname;
    let pm25History = [];
    eventSource.onmessage = function(event) { const data = JSON.parse(event.data); updateDisplay(data); updateChart(data.dust); };
    function updateDisplay(data) {
      document.getElementById('temp').textContent = data.temp; document.getElementById('humidity').textContent = data.hum;
      document.getElementById('pressure').textContent = data.pressure; document.getElementById('voc').textContent = data.voc;
      document.getElementById('dust').textContent = data.dust;
      const aqiEl = document.getElementById('aqi-text'); const subtitleEl = document.getElementById('status-subtitle');
      aqiEl.textContent = data.aqi;
      const statusMap = {'GOOD': {class: 'good', subtitle: 'Optimal air quality detected'}, 'MODERATE': {class: 'moderate', subtitle: 'Acceptable air quality'}, 'POOR': {class: 'poor', subtitle: 'Unhealthy for sensitive groups'}, 'VERY POOR': {class: 'very-poor', subtitle: 'Unhealthy air quality'}};
      const status = statusMap[data.aqi] || statusMap['GOOD']; aqiEl.className = 'aqi-primary ' + status.class; subtitleEl.textContent = status.subtitle;
      const ventBtn = document.getElementById('vent-btn'); const puriBtn = document.getElementById('purifier-btn');
      ventBtn.textContent = 'Ventilation: ' + (data.vent ? 'ON' : 'OFF'); ventBtn.className = 'control-btn ' + (data.vent ? '' : 'off');
      puriBtn.textContent = 'Purifier: ' + (data.puri ? 'ON' : 'OFF'); puriBtn.className = 'control-btn ' + (data.puri ? '' : 'off');
    }
    function updateChart(pm25) { pm25History.push(pm25); if (pm25History.length > 20) pm25History.shift(); pm25Chart.data.labels = pm25History.map((_, i) => i); pm25Chart.data.datasets[0].data = pm25History; pm25Chart.update('none'); }
    function toggleVent() { const btn = document.getElementById('vent-btn'); fetch('/vent?state=' + (btn.textContent.includes('OFF') ? '1' : '0')); }
    function togglePurifier() { const btn = document.getElementById('purifier-btn'); fetch('/purifier?state=' + (btn.textContent.includes('OFF') ? '1' : '0')); }
  </script>
</body>
</html>
)rawliteral";
}

void loop() {
  unsigned long now = millis();
  
  // === SMART PUSH BUTTON (4 Modes Cycle) ===
  bool buttonState = !digitalRead(CONTROL_BUTTON);
  if (buttonState && !lastButtonState && (now - lastButtonPress > DEBOUNCE_DELAY)) {
    lastButtonPress = now;
    buttonMode = (buttonMode + 1) % 4;
    
    switch(buttonMode) {
      case 0: 
        showAQI = true;
        Serial.println("🔥 BUTTON: AQI MODE");
        break;
      case 1:
        ventState = !ventState;
        digitalWrite(RELAY_VENT, ventState);
        Serial.println("🔧 BUTTON: Vent " + String(ventState ? "ON" : "OFF"));
        break;
      case 2:
        puriState = !puriState;
        digitalWrite(RELAY_PURIFIER, puriState);
        Serial.println("🔧 BUTTON: Purifier " + String(puriState ? "ON" : "OFF"));
        break;
      case 3:
        showAQI = false;
        Serial.println("📊 BUTTON: Sensors MODE");
        break;
    }
    
    // LED feedback
    digitalWrite(LED_BUILTIN, HIGH); delay(100); digitalWrite(LED_BUILTIN, LOW);
  }
  lastButtonState = buttonState;
  
  // Sensors
  if (!bme.performReading()) {
    Serial.println("❌ BME read failed");
    delay(1000);
    return;
  }
  
  temp = bme.temperature;
  hum = bme.humidity;
  pressure = bme.pressure / 100.0;
  voc = bme.gas_resistance / 1000.0;
  dust = readDust();
  
  aqiStatus = getAQIStatus(dust, voc);
  controlDevices(dust, voc);
  
  Serial.printf("T:%.1f°C H:%.0f%% P:%.0fhPa VOC:%.0fk Dust:%.0fug AQI:%s\n", 
                temp, hum, pressure, voc, dust, aqiStatus.c_str());
  
  // Dual OLED Mode
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    if (showAQI) {
      showAQIDisplay(dust, voc, aqiStatus);
    } else {
      updateDisplay(temp, hum, pressure, voc, dust);
    }
    lastDisplayUpdate = now;
  }
  
  // Web Live Update
  if (now - lastWebUpdate >= WEB_INTERVAL) {
    String json = "{\"temp\":" + String(temp,1) + 
                  ",\"hum\":" + String(hum,0) +
                  ",\"pressure\":" + String(pressure,0) +
                  ",\"voc\":" + String(voc,0) +
                  ",\"dust\":" + String(dust,0) +
                  ",\"aqi\":\"" + aqiStatus + "\"" +
                  ",\"vent\":" + String(ventState) +
                  ",\"puri\":" + String(puriState) + "}";
    events.send(json.c_str(), "message", millis());
    lastWebUpdate = now;
  }
  
  delay(80);
}
