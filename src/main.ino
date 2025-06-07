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

long cal_m = 1.0;
long cal_c = 0.0;

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

// Static display control
bool staticDisplayInitialized = false;
unsigned long lastStaticUpdate = 0;
const unsigned long staticUpdateInterval = 500; // Update every 500ms

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

// Static display functions
void clearAndHome() {
  Serial.print("\033[2J");  // Clear screen
  Serial.print("\033[H");   // Move cursor to home
}

void moveCursor(int row, int col) {
  Serial.print("\033[");
  Serial.print(row);
  Serial.print(";");
  Serial.print(col);
  Serial.print("H");
}

void printAtPosition(int row, int col, String text) {
  moveCursor(row, col);
  Serial.print(text);
  // Clear rest of line to remove old text
  Serial.print("\033[K");
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
  // Initialize static display layout once
  if (!staticDisplayInitialized) {
    clearAndHome();
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║              CABLE FAULT DETECTOR - BRIDGE MODE             ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.println("║ Parameter        │ Value        │ Unit    │ Status          ║");
    Serial.println("╠══════════════════┼──────────────┼─────────┼─────────────────╣");
    Serial.println("║ Bus Voltage      │              │ V       │                 ║");
    Serial.println("║ Current          │              │ mA      │                 ║");
    Serial.println("║ Read Frequency   │              │ Hz      │                 ║");
    Serial.println("║ Buffer Status    │              │         │                 ║");
    Serial.println("║ LED Interval     │              │ ms      │                 ║");
    Serial.println("║ Loop Count       │              │         │                 ║");
    Serial.println("╚══════════════════╧══════════════╧═════════╧═════════════════╝");
    Serial.println();
    Serial.println("Press Ctrl+C to exit monitoring mode");
    staticDisplayInitialized = true;
  }

  // Update only values at specific positions
  printAtPosition(6, 20, String(voltage, 3));
  printAtPosition(7, 20, String(current, 2));
  printAtPosition(8, 20, String(freqHz));
  printAtPosition(9, 20, bufferFilled ? "FULL" : "FILLING");
  printAtPosition(10, 20, String(interval));
  printAtPosition(11, 20, String(loopCounter));
  
  // Status indicators
  String voltageStatus = (voltage > 0.1) ? "ACTIVE" : "LOW";
  String currentStatus = (abs(current) > 1) ? "DETECTED" : "MINIMAL";
  
  printAtPosition(6, 43, voltageStatus);
  printAtPosition(7, 43, currentStatus);
  printAtPosition(8, 43, (freqHz > 0) ? "READING" : "IDLE");

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

  // Initialize static display layout once
  if (!staticDisplayInitialized) {
    clearAndHome();
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║            CABLE FAULT DETECTOR - FAULT ANALYSIS            ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.println("║ INPUT PARAMETERS                                             ║");
    Serial.println("╠══════════════════┬──────────────┬─────────┬─────────────────╣");
    Serial.println("║ Battery Voltage  │              │ V       │                 ║");
    Serial.println("║ Reference Resist │              │ Ohm     │                 ║");
    Serial.println("║ Cable Length     │              │ m       │                 ║");
    Serial.println("║ Measured Current │              │ mA      │                 ║");
    Serial.println("╠══════════════════╪══════════════╪═════════╪═════════════════╣");
    Serial.println("║ CALIBRATION DATA                                             ║");
    Serial.println("╠══════════════════┬──────────────┬─────────┬─────────────────╣");
    Serial.println("║ Cal. Multiplier  │              │         │                 ║");
    Serial.println("║ Cal. Offset      │              │         │                 ║");
    Serial.println("╠══════════════════╪══════════════╪═════════╪═════════════════╣");
    Serial.println("║ CALCULATED RESULTS                                           ║");
    Serial.println("╠══════════════════┬──────────────┬─────────┬─────────────────╣");
    Serial.println("║ Variable Resist. │              │ Ohm     │                 ║");
    Serial.println("║ Calibrated R_var │              │ Ohm     │                 ║");
    Serial.println("║ FAULT DISTANCE   │              │ m       │                 ║");
    Serial.println("║ Fault Percentage │              │ %       │                 ║");
    Serial.println("║ Loop Count       │              │         │                 ║");
    Serial.println("╚══════════════════╧══════════════╧═════════╧═════════════════╝");
    Serial.println();
    Serial.println("Press Ctrl+C to exit fault analysis mode");
    staticDisplayInitialized = true;
  }

  // Update input parameters
  printAtPosition(6, 20, String(v_bat, 3));
  printAtPosition(7, 20, String(R_ref, 1));
  printAtPosition(8, 20, String(cableLength, 1));
  printAtPosition(9, 20, String(current_mA, 2));
  
  // Update calibration data
  printAtPosition(13, 20, String((float)cal_m, 6));
  printAtPosition(14, 20, String((float)cal_c, 6));
  
  // Update calculated results
  printAtPosition(17, 20, String(R_var, 2));
  printAtPosition(18, 20, String(R_var_c, 2));
  printAtPosition(19, 20, String(s, 2));
  printAtPosition(20, 20, String((s/cableLength)*100, 1));
  printAtPosition(21, 20, String(loopCounter));
  
  // Status indicators
  String currentStatus = (abs(current_mA) > 1) ? "DETECTED" : "LOW";
  String faultStatus;
  if (s < cableLength * 0.1) faultStatus = "NEAR START";
  else if (s > cableLength * 0.9) faultStatus = "NEAR END";
  else faultStatus = "MID CABLE";
  
  printAtPosition(9, 43, currentStatus);
  printAtPosition(19, 43, faultStatus);

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
  display.print(R_var, 2);
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
  // Only show calibration fetch status during initialization
  static bool firstFetch = true;
  
  if (firstFetch) {
    Serial.println();
    Serial.println("[CALIB] Fetching calibration data from server...");
  }
  
  WiFiClientSecure client;
  client.setInsecure();  // Skip SSL certificate validation

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // <- penting!
  http.begin(client, scriptURL);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    if (firstFetch) {
      Serial.println("[CALIB] Server response received");
      Serial.printf("[CALIB] Response: %s\n", payload.c_str());
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      cal_m = doc["m"];
      cal_c = doc["c"];
      if (firstFetch) {
        Serial.println("[OK] Calibration parameters updated:");
        Serial.printf("      Multiplier (m): %.6f\n", (float)cal_m);
        Serial.printf("      Offset (c)    : %.6f\n", (float)cal_c);
      }
    } else {
      if (firstFetch) {
        Serial.printf("[ERROR] JSON parsing failed: %s\n", error.c_str());
      }
    }
  } else {
    if (firstFetch) {
      Serial.printf("[ERROR] HTTP request failed with code: %d\n", httpCode);
      if (httpCode > 0) {
        String errorResponse = http.getString();
        Serial.printf("[ERROR] Response: %s\n", errorResponse.c_str());
      }
    }
  }

  http.end();
  firstFetch = false;
}

void loop() {
  loopCounter++;

  // Fetch calibration periodically (silently after first time)
  fetchCalibration();

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
    int currentState = digitalRead(BUTTON_PIN) == LOW ? 0 : 1;
    if (currentState != state) {
      staticDisplayInitialized = false; // Reset display when mode changes
      state = currentState;
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
