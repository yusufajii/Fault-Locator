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

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Internal pull-up enabled
  pinMode(2, OUTPUT);
  Wire.begin();

  if (!ina219.begin()) {
    Serial.println("INA219 not found!");
    while (1);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("WiFi Connected!");
  fetchCalibration();



  Serial.println("System initialized.");
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
  Serial.print("Avg Vbus: ");
  Serial.print(voltage);
  Serial.print(" V, Current: ");
  Serial.print(current);
  Serial.print(" mA, Freq: ");
  Serial.print(freqHz);
  Serial.println(" Hz");

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
  float R_var_c = Rvar * cal_m + cal_c;
  float s = (R_var_c / (R_ref + R_var_c)) * 2.0 * cableLength;

  // Serial log
  Serial.println("=== LOCATING FAULT MODE ===");
  Serial.print("V_bat: "); Serial.print(v_bat); Serial.print(" V, ");
  Serial.print("Current: "); Serial.print(current_mA); Serial.print(" mA, ");
  Serial.print("R_ref: "); Serial.print(R_ref); Serial.print(" Ohm, ");
  Serial.print("Cable length: "); Serial.print(cableLength); Serial.println(" m");
  Serial.print("R_var: "); Serial.print(R_var); Serial.print(" Ohm, ");
  Serial.print("Fault location: "); Serial.print(s); Serial.println(" m");

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
  WiFiClientSecure client;
  client.setInsecure();  // Skip SSL certificate validation

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // <- penting!
  http.begin(client, scriptURL);

  Serial.println("[HTTP] Requesting calibration data...");
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Response: " + payload);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      cal_m = doc["m"];
      cal_c = doc["c"];
      Serial.println("Calibration loaded.");
    } else {
      Serial.print("Deserialization error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("[HTTP] GET failed, code: ");
    Serial.println(httpCode);
  }

  http.end();
}



void loop() {

  fetchCalibration();
  Serial.print(cal_m);
  Serial.println(cal_c);

  // Baca sensor
  float busVoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  updateBuffers(busVoltage, current_mA);

  readCounter++;

  // Hitung frekuensi baca setiap 1 detik
  if (millis() - lastFreqTime >= 1000) {
    readFrequency = readCounter;
    readCounter = 0;
    lastFreqTime = millis();
  }

  // Update tampilan setiap interval
  if (millis() - lastDisplayTime >= displayInterval) {
    lastDisplayTime = millis();
    int validSize = bufferFilled ? BUFFER_SIZE : bufferIndex;
    float avgVoltage = mean(busVoltageBuffer, validSize);
    float avgCurrent = mean(currentBuffer, validSize);
    //displayReadings(avgVoltage, avgCurrent ,readFrequency);
    
    if (digitalRead(BUTTON_PIN) == LOW) {
      state = 0;
    } else {
      state = 1;
    }
    if (state){
      displayFault(avgCurrent, v_bat, r_reff, cable_length);
    }
    else{
      displayReadings(avgVoltage, avgCurrent, readFrequency);
    }
    
    if(abs(avgCurrent)<1){
      interval = 150;
    }
    else if(abs(avgCurrent)<20){
      interval = 500;
    }
    else if(abs(avgCurrent)<100){
      interval = 1000;
    }
    else if(abs(avgCurrent)<200){
      interval = 2000;
    }
    else{
      interval = 10000000;
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Toggle the LED state
    ledState = !ledState;
    digitalWrite(2, ledState);
  }
  // Sedikit delay untuk kestabilan sistem
  delay(50);
}


