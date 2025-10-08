# 🌾 Smart Agriculture Project with ESP32 Mesh

## 📝 Introduction
This project is a **Smart Agriculture System** using **ESP32** devices connected via **Wi-Fi Mesh**. It allows real-time monitoring of environmental conditions in a farm and provides remote data management.

## ⚡ Key Features
- 🌡️ **DHT22**: measures temperature and humidity.
- 💡 **BH1750VFI**: measures light intensity.
- 💧 **Capacitive Soil Moisture Sensor**: measures soil moisture.
- 📡 Communication between ESP32 devices through **Wi-Fi Mesh**.
- ☁️ Sends data to a server or remote application for monitoring.
- 🤖 Can be extended to automate irrigation or provide alerts when values exceed thresholds.

## 🖥️ System Overview
- 🧑‍💻 **ESP32 Master**: collects data from nodes and sends it to the server.
- 📡 **ESP32 Nodes**: connected to sensors (DHT22, BH1750, soil moisture) and transmit data via mesh.

## 🛠️ Hardware Requirements
- ESP32 development boards (Master + Nodes)
- DHT22 temperature & humidity sensors
- BH1750VFI light sensors
- Capacitive soil moisture sensors
- Connecting wires, power supply

## 💻 Software Requirements
- Arduino IDE or PlatformIO
- ESP32 board libraries
- Wi-Fi Mesh libraries

## 🔧 How It Works
1. Each node reads data from its sensors periodically.
2. Data is sent to the master ESP32 via Wi-Fi Mesh.
3. The master can upload the data to a server or cloud for monitoring.
4. Alerts can be generated if sensor readings are out of range.

## 📜 License
This project is open-source and available under the MIT License.
