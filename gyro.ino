#include <Wire.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <HTTPClient.h>



const char* ssid = "YOUR SSID";
const char* password = "YOUR WIFI PASSWORD";
String BOTtoken = "YOUR Telegram BOT Token";
String chatID = "YOUR CHATID";

void sendTelegramMessage(String message) {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOTtoken + "/sendMessage?chat_id=" + chatID + "&text=" + message;
  
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.println("Message sent successfully!");
  } else {
    Serial.print("Error sending message. HTTP response: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

MPU6050 mpu;
int BUZZER = 25;
int16_t ax, ay, az;
float threshold = 0.5;  // Total g threshold for fall detection

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for Serial Monitor to open

  // Initialize I2C with ESP32 default pins
  Wire.begin(21, 22); // SDA = GPIO21, SCL = GPIO22

  Serial.println("Initializing MPU6050...");
  mpu.initialize();

  if (mpu.testConnection()) {
    Serial.println("MPU6050 connected.");
  } else {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }

  // Optional: calibration (takes a few seconds)
  mpu.CalibrateAccel(6);
  mpu.CalibrateGyro(6);
  Serial.println("Calibration done.");
  pinMode(BUZZER, OUTPUT);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
}

void loop() {
  mpu.getAcceleration(&ax, &ay, &az);

  // Convert raw values to 'g'
  float gX = ax / 16384.0;
  float gY = ay / 16384.0;
  float gZ = az / 16384.0;

  float totalG = sqrt(gX * gX + gY * gY + gZ * gZ);
  
  //Serial.println("Calibration done.");
  // Print every 200ms to show sensor is working
  Serial.print("gX: "); Serial.print(gX, 2);
  Serial.print(" | gY: "); Serial.print(gY, 2);
  Serial.print(" | gZ: "); Serial.print(gZ, 2);
  Serial.print(" | Total G: "); Serial.println(totalG, 2);
  if(totalG==0)
  {
    mpu.CalibrateAccel(6);
  mpu.CalibrateGyro(6);
   }

  // Detect vertical fall
  if (totalG < threshold) {
    Serial.println("FALLEN");
    delay(1000); // Wait to avoid multiple prints
  } 
  else if (gX >0.65) {
    Serial.println("FALLING TOWARDS RIGHT");
    
    digitalWrite(BUZZER, HIGH);
    delay(1000); // Wait to avoid multiple prints
    digitalWrite(BUZZER, LOW);
    sendTelegramMessage("FALLING TOWARDS RIGHT");
  } 
  else if (gX <-0.65) {
    Serial.println("FALLING TOWARDS LEFT");
    sendTelegramMessage("FALLING TOWARDS LEFT");
    digitalWrite(BUZZER, HIGH);
    delay(1000); // Wait to avoid multiple prints
    digitalWrite(BUZZER, LOW);
  }
  else if (gY >0.65) {
    Serial.println("FALLING TOWARDS FRONT");
    sendTelegramMessage("FALLING TOWARDS FRONT");
    digitalWrite(BUZZER, HIGH);
    delay(1000); // Wait to avoid multiple prints
    digitalWrite(BUZZER, LOW);
  } 
  else if (gY <-0.65) {
    Serial.println("FALLING TOWARDS BACK");
    sendTelegramMessage("FALLING TOWARDS BACK");
    digitalWrite(BUZZER, HIGH);
    delay(1000); // Wait to avoid multiple prints
    digitalWrite(BUZZER, LOW);
  } else {
    delay(200); // Regular delay when not fallen
  }
  
}
