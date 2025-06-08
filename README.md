# 🔧 Digital Murray Loop Tester for Cable Fault Detection

<p align="center">
  <img src="https://github.com/user-attachments/assets/8d1a1e95-7e3b-4ccc-beb8-c52c2cc64029" alt="Digital Murray Loop Tester" />
</p>

A smart cable fault detection tool based on the **Wheatstone Bridge principle**, enhanced with **modern automation** for precise and autonomous fault localization.

---

### 🚀 Key Features

- ✅ Autonomous fault distance calculation  
- ✅ 1Ω shunt precision using **INA219 Current Sensor**  
- ✅ Real-time display with **ESP32 + 0.96" OLED**  
- ✅ Dual operation modes:  
  &nbsp;&nbsp;&nbsp;&nbsp;• **Current Measurement Mode**  
  &nbsp;&nbsp;&nbsp;&nbsp;• **Fault Distance Mode**  
- ✅ Fully **PCB-integrated design** (no jumper wires)  
- ✅ IoT-enabled with remote monitoring via **Blynk**  
- ✅ **Open-source** firmware (fully customizable)  

---

### 📐 Technical Specifications

| Parameter                  | Value                          |
|---------------------------|---------------------------------|
| **MCU Supply Voltage**     | 3.3V / 5V                      |
| **Bridge Supply Voltage**  | 9V                             |
| **Current Consumption**    | 300–500mA (MCU), 0–250mA (Bridge) |
| **Variable Resistance**    | 0–1kΩ (via Potentiometer)      |
| **Reference Resistance**   | Fixed 1000Ω                    |
| **Probes**                 | 3 (2 for bridge, 1 for GND)    |
| **Input**                  | Push button (mode switch), programmable cable length |
| **Output**                 | Bridge current, R<sub>var</sub>, calculated fault distance |

---

### 🧪 Lab-Tested Performance

- 📏 Tested on **300m cable setup**
- 🎯 Accuracy: ±0.1m
- 📉 Error Margin: 0–10%
- 📶 Sensitivity: 0.2
- 📍 Distance Offset: ±7m
- 🧲 Resistance Offset: ±200Ω

---

### 🛠️ Product Preview

<p align="center">
  <img src="https://github.com/user-attachments/assets/f6ce0b2e-307a-4972-8ad2-fae7a2ec3254" alt="Device Image 1" />
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/7056b86c-ccc9-4277-804f-b2084383208c" alt="Device Image 2" />
</p>

---

### 📊 Circuit Schematic

<p align="center">
  <img src="https://github.com/user-attachments/assets/3e8a333b-037c-4e74-8149-5c72a6591873" alt="Circuit Schematic" />
</p>

---

### 💡 Notes

This project bridges classic electrical principles with modern microcontroller and IoT capabilities. It is ideal for labs, diagnostics, or academic exploration of fault localization systems.

---

