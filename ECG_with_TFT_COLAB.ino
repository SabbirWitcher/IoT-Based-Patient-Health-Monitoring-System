#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT display pin definitions
#define TFT_CS     5
#define TFT_RST    4
#define TFT_DC     2
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const char* ssid = "YOUR SSID";
const char* password = "YOUR WIFI PASSWORD";

// Display and graph area
#define TFT_WIDTH   128
#define TFT_HEIGHT  160
#define GRAPH_TOP   20
#define GRAPH_HEIGHT 100
#define GRAPH_BOTTOM (GRAPH_TOP + GRAPH_HEIGHT)


const char* serverName = "YOUR SCRIPT LINK";

const int ECG_PIN = 36;   // Analog input pin (VP/GPIO36)
const int LO_PLUS = 15;   // Leads-off detection LO+
const int LO_MINUS = 14;  // Leads-off detection LO-

void printValue(float value);
void sendData(int* buffer, int length);



uint16_t graphBuffer[TFT_WIDTH];  // Store vertical positions of waveform

// Smoothing parameters
#define SMOOTHING_WINDOW 5
int ecgReadings[SMOOTHING_WINDOW];
int readingIndex = 0;
int readingSum = 0;

// Debug flag to bypass leads-off (set to true for testing)
#define BYPASS_LEADS_OFF false // Set to true to test raw ECG signal

// Function to smooth ECG signal
int smoothECG(int rawValue) {
  readingSum -= ecgReadings[readingIndex];
  ecgReadings[readingIndex] = rawValue;
  readingSum += rawValue;
  readingIndex = (readingIndex + 1) % SMOOTHING_WINDOW;
  return readingSum / SMOOTHING_WINDOW;
}

// Function to plot ECG on ST7735
void plotECG(int value) {
  // Map smoothed value (0-4095) to graph height (GRAPH_TOP to GRAPH_BOTTOM)
  int y = map(value, 0, 4095, GRAPH_BOTTOM, GRAPH_TOP);
  graphBuffer[TFT_WIDTH - 1] = y;

  // Shift buffer left
  for (int i = 0; i < TFT_WIDTH - 1; i++) {
    graphBuffer[i] = graphBuffer[i + 1];
  }

  // Clear graph area
  tft.fillRect(1, GRAPH_TOP, TFT_WIDTH - 2, GRAPH_HEIGHT, ST77XX_BLACK);

  // Draw grid
  for (int i = GRAPH_TOP; i <= GRAPH_BOTTOM; i += 20) {
    tft.drawFastHLine(0, i, TFT_WIDTH, ST77XX_BLUE);
  }
  for (int i = 0; i < TFT_WIDTH; i += 20) {
    tft.drawFastVLine(i, GRAPH_TOP, GRAPH_HEIGHT, ST77XX_BLUE);
  }

  // Draw waveform
  for (int i = 1; i < TFT_WIDTH; i++) {
    tft.drawLine(i - 1, graphBuffer[i - 1], i, graphBuffer[i], ST77XX_GREEN);
  }

  // Display current value
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.print("ECG: ");
  tft.print(value);
  tft.print("  ");
}



const int BUFFER_SIZE = 50;
int dataBuffer[BUFFER_SIZE];
int bufferIndex = 0;  

void setup() {
  Serial.begin(115200);
  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);
  // Initialize ST7735
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); // Adjust if needed (0-3)
  tft.fillScreen(ST77XX_BLACK);
  // Draw graph border and label
  tft.drawRect(0, GRAPH_TOP - 1, TFT_WIDTH, GRAPH_HEIGHT + 2, ST77XX_WHITE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.print("ECG Real-Time");

  // Initialize graph buffer
  for (int i = 0; i < TFT_WIDTH; i++) {
    graphBuffer[i] = GRAPH_BOTTOM;
  }

  // Initialize smoothing array
  for (int i = 0; i < SMOOTHING_WINDOW; i++) {
    ecgReadings[i] = 2048; // Midpoint of ADC range
  }
  readingSum = 2048 * SMOOTHING_WINDOW;

  Serial.println("Setup complete. Monitoring ECG...");
  Serial.println("LO+ | LO- | Raw ECG | Smoothed ECG | Plotter");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

void loop() {
  int analogValue = analogRead(ECG_PIN);
  int rawValue = analogValue;
  int smoothedValue = smoothECG(rawValue);
  // Read leads-off pins for debugging
  int loPlus = digitalRead(LO_PLUS);
  int loMinus = digitalRead(LO_MINUS);

  // Print debug info
  Serial.print(loPlus);
  Serial.print(" | ");
  Serial.print(loMinus);
  Serial.print(" | ");
  Serial.print(rawValue);
  Serial.print(" | ");
  Serial.print(smoothedValue);

  // Check for leads-off condition
  if ((loPlus == 1 || loMinus == 1) && !BYPASS_LEADS_OFF) {
    Serial.println(" | Leads Off");
    tft.fillRect(1, GRAPH_TOP, TFT_WIDTH - 2, GRAPH_HEIGHT, ST77XX_BLACK);
    tft.setCursor(10, GRAPH_BOTTOM + 10);
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.println("LEADS OFF");
  } else {
    // Output for Serial Plotter (scaled to 0-100)
    int plotValue = map(smoothedValue, 0, 4095, 0, 100);
    Serial.print(" | ");
    Serial.println(plotValue);
    plotECG(smoothedValue);
  }

  delay(10); // ~100 Hz sampling
  

  
  // Check for leads-off condition
  if (digitalRead(LO_PLUS) == 1 || digitalRead(LO_MINUS) == 1) {
    // Do nothing or optionally print error
  } else {
    Serial.println(analogValue);
    printValue(analogValue);

    // Add value to buffer
    dataBuffer[bufferIndex++] = analogValue;

    // When buffer is full, send all values
    if (bufferIndex >= BUFFER_SIZE) {
      sendData(dataBuffer, BUFFER_SIZE);
      bufferIndex = 0;  // Reset buffer
    }
  }

  delay(10); // 100 samples per second
}

void printValue(float value) {
  Serial.print(value);
}

void sendData(int* buffer, int length) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String payload = "";

    for (int i = 0; i < length; i++) {
      payload += String(buffer[i]);
      if (i < length - 1) payload += ",";
    }

    String url = String(serverName) + "?value=" + payload;
    http.begin(url);
    int httpResponseCode = http.GET();
    Serial.println("Response: " + String(httpResponseCode));
    http.end();
  }
}
