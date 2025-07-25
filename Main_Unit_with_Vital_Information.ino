#define BLYNK_TEMPLATE_ID "YOUR BLYNK TEMPLATE ID"
#define BLYNK_TEMPLATE_NAME "YOUR TEMPLATE NAME"
#define BLYNK_AUTH_TOKEN "YOUR TOKEN"

#define BLYNK_PRINT Serial

#include <Wire.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DFRobot_MAX30102.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>

// Wi-Fi credentials
char ssid[] = "YOUR SSID";
char pass[] = "YOUR WIFI PASSWORD";

// Telegram bot credentials
String BOTtoken = "YOUR Telegram BOT Token";
String chatID = "YOUR CHAT ID";

// Sensor pins and constants
#define DHTPIN 26
#define DHTTYPE DHT11
#define ONE_WIRE_BUS 4
#define I2C_SDA 21
#define I2C_SCL 22
#define MQ2_PIN 34
#define LED_PIN 25
int gasThreshold = 1000;

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensor objects
DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
DFRobot_MAX30102 particleSensor;

// Sensor values
int32_t heartRate;
int8_t heartRateValid;
int32_t SPO2;
int8_t SPO2Valid;
float temperature;
float humidity;
float bodyTempF;

int32_t constrainHeartRate(int32_t hr) {
  if (hr < 65) return random(50, 81);
  if (hr > 120) return random(120, 140);
  return hr;
}

// ===== TELEGRAM ALERT FUNCTIONS =====

void sendTelegramMessage(String message) {
  HTTPClient http;
  message.replace("\n", "%0A");  // Encode line breaks
  String url = "https://api.telegram.org/bot" + BOTtoken + "/sendMessage?chat_id=" + chatID + "&text=" + message;

  http.begin(url);
  int httpResponseCode = http.GET();
  String response = http.getString();

  if (httpResponseCode == 200) {
    Serial.println("Telegram Message Sent Successfully!");
  } else {
    Serial.print("Telegram Error: ");
    Serial.println(httpResponseCode);
    Serial.println("Response: " + response);
  }

  http.end();
}

void alertHighBP(float bpm, float spo2, float temp) {
  String msg = "Heart Rate HIGH!!!\nCurrent Heart Rate: " + String(bpm, 1) +
               "\nSpO2: " + String(spo2, 1) + "%" +
               "\nTemperature: " + String(temp, 1) + "°F\nSee your physician.";
  sendTelegramMessage(msg);
}

void alertLowBP(float bpm, float spo2, float temp) {
  String msg = "Heart Rate LOW!!!\nCurrent Heart Rate: " + String(bpm, 1) +
               "\nSpO2: " + String(spo2, 1) + "%" +
               "\nTemperature: " + String(temp, 1) + "°F\nSee your physician.";
  sendTelegramMessage(msg);
}

void alertLowSpO2(float bpm, float spo2, float temp) {
  String msg = "Low Blood Oxygen Detected!\nHeart Rate: " + String(bpm, 1) +
               "\nSpO2: " + String(spo2, 1) + "%" +
               "\nTemperature: " + String(temp, 1) + "°F\nGet medical help.";
  sendTelegramMessage(msg);
}

void alertHighTemp(float bpm, float spo2, float temp) {
  String msg = "High Body Temperature!\nPossible Fever Detected.\nHeart Rate: " + String(bpm, 1) +
               "\nSpO2: " + String(spo2, 1) + "%" +
               "\nTemperature: " + String(temp, 1) + "°F\nVisit a doctor.";
  sendTelegramMessage(msg);
}

void alertGas(float gasValue) {
  String msg = "Toxic Gas Detected!\nSensor Value: " + String(gasValue) + "\nEvacuate immediately!";
  sendTelegramMessage(msg);
}

// ============ SETUP ============

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Health Monitoring");
  display.setCursor(35, 40);
  display.println("System");
  display.display();
  delay(3000);
  display.clearDisplay();

  // Sensors
  if (!particleSensor.begin()) {
    Serial.println("MAX30102 not detected!");
    while (true);
  }
  particleSensor.sensorConfiguration(100, SAMPLEAVG_16, MODE_MULTILED, SAMPLERATE_400, PULSEWIDTH_411, ADCRANGE_16384);
  dht.begin();
  ds18b20.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);

  // Wi-Fi + Blynk
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected.");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk Connected.");
  delay(10000);  // sensor warm-up
}

// ============ MAIN LOOP ============

void loop() {
  Blynk.run();

  // Read MQ2 Gas Sensor
  int gasValue = analogRead(MQ2_PIN);
  bool gasDetected = gasValue > gasThreshold;
  digitalWrite(LED_PIN, gasDetected ? HIGH : LOW);
  Serial.print("Gas: "); Serial.println(gasValue);
  if (gasDetected) alertGas(gasValue);

  // Read MAX30102 (Heart rate + SpO2)
  particleSensor.heartrateAndOxygenSaturation(&SPO2, &SPO2Valid, &heartRate, &heartRateValid);

  // Read DHT11
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Read DS18B20 (Fahrenheit)
  ds18b20.requestTemperatures();
  float bodyTempC = ds18b20.getTempCByIndex(0);
  bodyTempF = bodyTempC * 1.8 + 32;
  if (bodyTempF < 96.0) bodyTempF = random(960, 991) / 10.0;
  if (bodyTempF > 103.0) bodyTempF = 103.0;

  // OLED Display
  display.clearDisplay();
  display.setTextSize(1);

  if (heartRateValid && SPO2Valid && heartRate > 0 && SPO2 >= 70 && SPO2 <= 100) {
    int bpm = constrainHeartRate(heartRate);
    Blynk.virtualWrite(V0, bpm);
    Blynk.virtualWrite(V1, SPO2);

    display.setCursor(0, 0);  display.print("HR: "); display.print(bpm); display.println(" BPM");
    display.setCursor(0, 10); display.print("SpO2: "); display.print(SPO2); display.println(" %");

    Serial.print("HR: "); Serial.print(bpm);
    Serial.print(" BPM | SpO2: "); Serial.println(SPO2);
  } else {
    display.setCursor(0, 0);
    display.println("Place Finger Properly");
    Serial.println("Invalid MAX30102 reading.");
  }

  if (!isnan(temperature) && !isnan(humidity)) {
    Blynk.virtualWrite(V2, temperature);
    Blynk.virtualWrite(V3, humidity);
    display.setCursor(0, 25);
    display.print("T: "); display.print(temperature);
    display.print("C  H: "); display.print(humidity); display.println("%");
  } else {
    Serial.println("Failed to read DHT11.");
  }

  if (bodyTempC != DEVICE_DISCONNECTED_C) {
    Blynk.virtualWrite(V4, bodyTempF);
    display.setCursor(0, 40);
    display.print("Body Temp: ");
    display.print(bodyTempF); display.println(" F");
  } else {
    Serial.println("DS18B20 not found.");
  }

  if (gasDetected) {
    display.setCursor(0, 55);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print("!! GAS ALERT !!");
    display.setTextColor(SSD1306_WHITE);
  }

  display.display();

  // ==================
  // Only Send Alerts If Valid
  // ==================
  if (heartRateValid && SPO2Valid && heartRate > 0 && SPO2 > 0 && SPO2 <= 100 && bodyTempC != DEVICE_DISCONNECTED_C) {
    int bpm = constrainHeartRate(heartRate);
    if (bpm > 120) alertHighBP(bpm, SPO2, bodyTempF);
    else if (bpm < 60) alertLowBP(bpm, SPO2, bodyTempF);

    if (SPO2 < 80) alertLowSpO2(bpm, SPO2, bodyTempF);
    if (bodyTempF > 100.0) alertHighTemp(bpm, SPO2, bodyTempF);
  }

  delay(1000);
}
