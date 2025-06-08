#define BUTTON_PIN 18

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

const char* ssid = "La belle Ucup's";
const char* password = "thelovelyucup";
const char* scriptURL = "https://script.google.com/macros/s/AKfycbwosf8tYSlJzCFmv3odfdeevN1cSsr4fyg3gdRUL8GeZu-9vRPVYRtte6bwU5wFwnkLXA/exec";

float cal_m = 1.0;
float cal_c = 0.0;

int state = 1;
unsigned long loopCounter = 0;

unsigned long previousMillis = 0;
long interval = 1000;  // Blink interval in milliseconds (1 sec)
bool ledState = false;

float cable_length = 100;
float r_reff = 1000;
float v_bat = 7.180;

Adafruit_INA219 ina219(0x41);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const unsigned long displayInterval = 1000;  // update setiap 1000 ms
unsigned long lastDisplayTime = 0;

// Statistik buffer
const int BUFFER_SIZE = 100;
float busVoltageBuffer[BUFFER_SIZE];
float currentBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool bufferFilled = false;

// Frekuensi pembacaan
unsigned long readCounter = 0;
unsigned long lastFreqTime = 0;
int readFrequency = 0;

// Function to print separator lines
void printSeparator(char character = '=', int length = 60) {
  Serial.println();
  for (int i = 0; i < length; i++) {
    Serial.print(character);
  }
  Serial.println();
}

void printSubSeparator(char character = '-', int length = 40) {
  for (int i = 0; i < length; i++) {
    Serial.print(character);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  printSeparator('*', 70);
  Serial.println("         CABLE FAULT DETECTOR - SYSTEM INITIALIZATION");
  printSeparator('*', 70);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Internal pull-up enabled
  pinMode(2, OUTPUT);
  Wire.begin();

  Serial.println("[INIT] Configuring GPIO pins...");
  Serial.println("       - Button PIN: 18 (INPUT_PULLUP)");
  Serial.println("       - LED PIN: 2 (OUTPUT)");
  Serial.println("       - I2C Bus initialized");

  Serial.println();
  Serial.println("[INIT] Initializing INA219 current sensor...");
  if (!ina219.begin()) {
    Serial.println("[ERROR] INA219 sensor not found! System halted.");
    while (1);
  }
  Serial.println("[OK] INA219 sensor initialized successfully");

  Serial.println();
  Serial.println("[INIT] Initializing OLED display...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] OLED display not found! System halted.");
    while (1);
  }
  Serial.println("[OK] OLED display initialized successfully");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  Serial.println();
  Serial.println("[INIT] Connecting to WiFi...");
  Serial.print("       SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  Serial.print("       Connection status: ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("[OK] WiFi connected successfully");
  Serial.print("     IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println();
  Serial.println("[INIT] Fetching calibration parameters...");
  fetchCalibration();

  printSeparator('=', 70);
  Serial.println("           SYSTEM READY - STARTING MAIN LOOP");
  printSeparator('=', 70);
  Serial.println();
}

float mean(const float *buffer, int size) {
  float sum = 0;
  for (int i = 0; i < size; i++) {
    sum += buffer[i];
  }
  return sum / size;
}

void updateBuffers(float voltage, float current) {
  busVoltageBuffer[bufferIndex] = voltage;
  currentBuffer[bufferIndex] = current;
  bufferIndex++;
  if (bufferIndex >= BUFFER_SIZE) {
    bufferIndex = 0;
    bufferFilled = true;
  }
}

void displayReadings(float voltage, float current, int freqHz) {
  // Professional serial output for bridge parameters
  Serial.println();
  printSubSeparator('-', 50);
  Serial.println("BRIDGE PARAMETERS MONITORING");
  printSubSeparator('-', 50);
  Serial.printf("Bus Voltage    : %8.3f V\n", voltage);
  Serial.printf("Current        : %8.2f mA\n", current);
  Serial.printf("Read Frequency : %8d Hz\n", freqHz);
  Serial.printf("Buffer Status  : %s\n", bufferFilled ? "FULL" : "FILLING");
  printSubSeparator('-', 50);

  display.clearDisplay();

  // Bagian atas OLED (kuning)
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("Cable Fault Detector");

  // Garis horizontal pemisah
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Mode pengukuran
  display.setCursor(2, 16);
  display.setTextSize(1);
  display.print("Bridge Parameters : ");
  // Frame pinggir layar
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  // Tegangan Vbus kecil
  display.setCursor(4, 30);
  display.setTextSize(1);
  display.print(voltage, 2);
  display.print(" V");

  // Arus yang lebih menonjol (besar, tengah kanan)
  display.setTextSize(2);
  display.setCursor(4, 44);
  display.print(current, 2);  // Skala tampilan (visual only)
  display.setTextSize(1);
  display.print(" mA");

  // Frekuensi di kanan bawah
  display.setCursor(90, 54);
  display.setTextSize(1);
  display.print(freqHz);
  display.print(" Hz");

  display.display();
}

void displayFault(float current_mA, float v_bat, float R_ref, float cableLength) {
  // Hitung variabel
  float current_A = abs(current_mA);
  float R_var = v_bat * 1000 / current_A;
  float R_var_c = R_var * cal_m + cal_c;
  float s = (R_var_c / (R_ref + R_var_c)) * 2.0 * cableLength;

  // Professional serial output for fault analysis
  Serial.println();
  printSubSeparator('*', 50);
  Serial.println("FAULT LOCATION ANALYSIS");
  printSubSeparator('*', 50);
  
  Serial.println("INPUT PARAMETERS:");
  Serial.printf("  Battery Voltage  : %8.3f V\n", v_bat);
  Serial.printf("  Reference Resist.: %8.1f Ohm\n", R_ref);
  Serial.printf("  Cable Length     : %8.1f m\n", cableLength);
  Serial.printf("  Measured Current : %8.2f mA\n", current_mA);
  
  Serial.println();
  Serial.println("CALIBRATION DATA:");
  Serial.printf("  Cal. Multiplier  : %8.3f\n", (float)cal_m);
  Serial.printf("  Cal. Offset      : %8.3f\n", (float)cal_c);
  
  Serial.println();
  Serial.println("CALCULATED VALUES:");
  Serial.printf("  Variable Resist. : %8.2f Ohm\n", R_var);
  Serial.printf("  Calibrated R_var : %8.2f Ohm\n", R_var_c);
  Serial.printf("  Fault Distance   : %8.2f m\n", s);
  Serial.printf("  Fault Percentage : %8.1f%%\n", (s/cableLength)*100);
  
  printSubSeparator('*', 50);

  display.clearDisplay();

  // Header kuning
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("Cable Fault Detector");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Mode
  display.setCursor(2, 16);
  display.print("Locating Fault");

  // Frame
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  // Rvar
  display.setCursor(4, 30);
  display.setTextSize(1);
  display.print("Rvar: ");
  display.print(R_var_c, 2);
  display.print(" Ohm "); // Ohm

  // Current
  display.setCursor(4, 38);
  display.print("I: ");
  display.print(current_mA, 1);
  display.print(" mA");

  // Lokasi Fault (besar) + rasio
  display.setTextSize(2);
  display.setCursor(4, 48);
  display.print(s, 1);
  display.setTextSize(1);
  display.setCursor(75, 56);
  display.print("/");
  display.print((int)cableLength);
  display.print(" m ");
  
  display.display();
}

void fetchCalibration() {
  Serial.println();
  Serial.println("[CALIB] Fetching calibration data from server...");
  
  WiFiClientSecure client;
  client.setInsecure();  // Skip SSL certificate validation

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // <- penting!
  http.begin(client, scriptURL);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("[CALIB] Server response received");
    Serial.printf("[CALIB] Response: %s\n", payload.c_str());

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      cal_m = doc["m"];
      cal_c = doc["c"];
      cable_length = doc["dis"];
      v_bat = doc["vbat"];

      Serial.println("[OK] Calibration parameters updated:");
      Serial.printf("      Multiplier (m): %.6f\n", (float)cal_m);
      Serial.printf("      Offset (c)    : %.6f\n", (float)cal_c);
    } else {
      Serial.printf("[ERROR] JSON parsing failed: %s\n", error.c_str());
    }
  } else {
    Serial.printf("[ERROR] HTTP request failed with code: %d\n", httpCode);
    if (httpCode > 0) {
      String errorResponse = http.getString();
      Serial.printf("[ERROR] Response: %s\n", errorResponse.c_str());
    }
  }

  http.end();
}

void loop() {
  loopCounter++;

  // Sensor readings
  float busVoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  updateBuffers(busVoltage, current_mA);

  readCounter++;

  // Calculate reading frequency every second
  if (millis() - lastFreqTime >= 1000) {
    readFrequency = readCounter;
    readCounter = 0;
    lastFreqTime = millis();
  }

  // Update display at intervals
  if (millis() - lastDisplayTime >= displayInterval) {
    lastDisplayTime = millis();
    int validSize = bufferFilled ? BUFFER_SIZE : bufferIndex;
    float avgVoltage = mean(busVoltageBuffer, validSize);
    float avgCurrent = mean(currentBuffer, validSize);
    
    // Button state check
    if (digitalRead(BUTTON_PIN) == LOW) {
      if (state == 1) {
        Serial.println();
        Serial.println("[INPUT] Button pressed - Switching to MONITORING mode");
      }
      state = 0;
    } else {
      if (state == 0) {
        Serial.println();
        Serial.println("[INPUT] Button released - Switching to FAULT DETECTION mode");
      }
      state = 1;
    }
    
    // Display appropriate mode
    if (state) {
      displayFault(avgCurrent, v_bat, r_reff, cable_length);
    } else {
      displayReadings(avgVoltage, avgCurrent, readFrequency);
    }
    
    // Dynamic LED blink interval based on current
    long newInterval;
    if (abs(avgCurrent) < 1) {
      newInterval = 150;
    } else if (abs(avgCurrent) < 20) {
      newInterval = 500;
    } else if (abs(avgCurrent) < 100) {
      newInterval = 1000;
    } else if (abs(avgCurrent) < 200) {
      newInterval = 2000;
    } else {
      newInterval = 10000000;
    }
    
    if (newInterval != interval) {
      interval = newInterval;
      Serial.println();
      Serial.printf("[LED] Blink interval updated to %ld ms (Current: %.2f mA)\n", 
                    interval, avgCurrent);
    }
  }

  // LED blinking control
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(2, ledState);
  }
  
  // System stability delay
  delay(50);
}
