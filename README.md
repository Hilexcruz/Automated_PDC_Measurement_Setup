# ⚡ Automated Polarization-Depolarization Current (PDC) Measurement System

### Bachelor’s Thesis Project  
**Title:** Automation of a Measurement Setup to Analyze the Conductivity of Insulating Materials with the PDC Method  
**By:** [Hilary Ifeanyi Martins-Udeze]

---

## 📘 Project Overview

This project involved the **automation of a PDC (Polarization-Depolarization Current) measurement setup** to investigate the **conductivity behavior of insulating materials** under electrical stress. The project aimed to replace manual measurement routines with a robust, **microcontroller-based system** that can precisely control voltage application, monitor environmental variables, and record electrical responses.

The result is a **fully functional, modular test platform** using Arduino microcontroller and sensors, designed to control, measure, and log current responses in test samples under polarization and depolarization conditions.

---

## 🎯 Project Objectives

- Automate the entire PDC measurement process
- Improve repeatability and accuracy in data acquisition
- Enable real-time control and monitoring of:
  - Applied voltage and current response
  - Environmental parameters (temperature, pressure, etc.)
- Design and implement a **microcontroller-based control loop**
- Develop modular Arduino code to manage various sensor inputs and control logic

---

## 🧩 Features

✅ Designed and assembled both **breadboard and final soldered hardware versions**
✅ Designed, simulated, and built a **custom double-layer PCB**  
✅ Automated the entire PDC testing workflow with **Arduino + RTC-based control**  
✅ **PDC measurement automation** using Arduino  
✅ **Measurement modes** (Single & Multiple)  
✅ **Temperature & pressure sensors** for environmental monitoring    
✅ **Feedback-based enabled, voltage ramping & holding**  
✅ **RTC-based time synchronization** for time-controlled measurements  
✅ **Modular codebase** supporting multiple versions and test modes  
✅ **Compact final hardware prototype** with safety integrations  

---

## 📐 Hardware and PCB Design

This was not just a coding or automation project — it involved deep electronics engineering:

- Created the **circuit schematic** in EasyEDA
- Designed and laid out a **custom 2-layer PCB** with labeled silkscreen and power traces
- Selected and placed **high-voltage relays, resistors, capacitors**, and **sensor headers**
- Ordered and soldered the PCB manually to integrate with Arduino microcontrollers
- Verified board functionality with multimeter, oscilloscope, and test loads
- Integrated **decoupling capacitors**, **flyback diode protections**, and **thermal cutoff**

This phase showed mastery in:
- Power electronics
- Embedded hardware design
- Analog signal conditioning
- Hardware-software integration

---

## 🧪 Background

PDC methods are used to evaluate **conductivity and dielectric behavior** in insulating materials like polymers or insulation oils. When a voltage is applied, polarization current is measured; after removing the voltage, depolarization current is monitored.

The setup simulates this process by:
- Applying a defined voltage using a custom high-voltage generator
- Measuring the current through the test sample
- Recording time-stamped current values using Arduino-based analog-to-digital conversion
- Stopping the process based on temperature, time, or current thresholds

---

## 🧩 Core Features

| Feature                          | Description                                               |
|----------------------------------|-----------------------------------------------------------|
| ✅ PCB Design                    | Custom board with relays, headers, and filtering          |
| ✅ Modular Arduino Code         | Five versions (V1–V5) with step-by-step improvements       |
| ✅ Current & Temperature Sensors | ACS712, LM35, DHT for safety monitoring                   |
| ✅ Real-Time Clock (RTC)         | DS3231 for scheduled, accurate measurements               |
| ✅ BLE Integration               | Broadcast sensor data wirelessly for monitoring           |
| ✅ Fault Detection               | Auto shutdown on overheating or overcurrent               |
| ✅ Manual + Serial Control       | Serial command interface for debugging and control        |
| ✅ High Voltage Switching        | Controlled ramp-up/down using solid-state relay logic     |

---

## ⚙️ Technical Components

| Component              | Role                                                             |
|------------------------|------------------------------------------------------------------|
| Arduino Uno/Nano       | Microcontroller for logic, control, and communication            |
| DS3231 RTC             | Real-time clock for precise timing and synchronization           |
| HV+ / HV– relays       | High-voltage switching for applying/removing test voltage        |
| LM35 / DHT sensors     | Temperature sensing for environment & board                      |
| Pressure / Mic sensors | Monitor changes in environmental pressure or acoustic response   |
| ACS712 current sensor  | Measures small currents during PDC processes                     |
| BLE module (e.g. HM-10)| Wireless broadcasting of sensor data to receiver/mobile interface|

---

## 🧾 Conclusion

This project demonstrates the successful development of a fully automated PDC measurement system that blends **hardware design**, **embedded systems programming**, and **real-time data acquisition** into one cohesive platform. From designing and fabricating a custom PCB to writing modular Arduino code and integrating fault-tolerant sensor logic, the project showcases full-stack capabilities in electronic system development.

Whether for academic research, industrial testing, or future IoT-based diagnostic tools, this system is a foundation for scalable, modular, and intelligent test automation in electrical insulation analysis.

If you found this project interesting, feel free to fork it, open issues, or reach out for collaboration.  
Thanks for reading!

---

## 🤝 Contributing

If you'd like to contribute, improve the system, or adapt it for other measurement techniques, feel free to fork the repo and submit a pull request.

## 📬 Contact

📧 Email: hilarymartinsudeze@gmail.com  
🔗 LinkedIn: [My LinkedIn](https://linkedin.com/in/hilarymu)  

