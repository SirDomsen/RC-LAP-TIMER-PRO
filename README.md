# RC Lap Timer Pro 🏎️⏱️

A standalone, Wi-Fi-based infrared lap timing system for RC cars built on the **ESP8266** with an integrated web server.

---

## 🌟 Features

* **Infrared Detection:** Precise transponder signal acquisition using TSOP4838 sensors.
* **Integrated Web Server:** Live display of lap times, best laps, and standings directly in your browser (no app installation required).
* **Multiple Race Modes:** 
  * Free Practice
  * Qualifying (Time-based)
  * Race (Lap- or Time-based)
* **Dynamic Storage:** Saves driver names, car profiles, lap histories, and session results to flash memory via LittleFS.
* **Flexible Wi-Fi Connectivity:** Creates its own Access Point or connects to an existing home network.
* **Over-The-Air (OTA) Updates:** Convenient firmware and web interface updates straight through the browser.

---

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP8266 (e.g., NodeMCU V2/V3 or Wemos D1 Mini)
* **IR Receiver:** TSOP4838 (connected to Pin `D2` / GPIO4)
* **LEDs (optional status indicators):**
  * Green LED (Ready/Status): Pin `D5` (GPIO14)
  * Yellow LED (IR Signal): Pin `D6` (GPIO12)

---

## 🚀 Installation & Setup

### 1. Clone the Repository
```bash
git clone [https://github.com/SirDomsen/RC-LAP-TIMER-PRO.git](https://github.com/SirDomsen/RC-LAP-TIMER-PRO.git)
cd RC-LAP-TIMER-PRO


2. Configure Wi-Fi Credentials
Create a file named secrets.h inside the src/ directory with your local Wi-Fi credentials:

C++
#ifndef SECRETS_H
#define SECRETS_H

#define HOME_SSID   "YourWifiName"
#define HOME_PASS   "YourWifiPassword"

#endif
(Note: The secrets.h file is excluded via .gitignore and will never be pushed to GitHub).

3. Build & Flash
This project is optimized for PlatformIO:

Open the project folder in VS Code with the PlatformIO extension installed.

Connect your ESP8266 via USB.

Upload both the Firmware and the Filesystem (LittleFS image).

💻 Usage
Connect to the timer's Wi-Fi network or navigate to the following URL in your web browser:
http://192.168.0.8 or http://laptimer.local

Configure drivers, car names, and deadtime parameters in the dashboard.

Start a session and track lap times in real-time!

📄 License
This project is licensed under the MIT License.
