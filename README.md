# TimOS: ESP32C3-Pro Smart Timer

A premium, feature-rich, ultra-low-power standalone operating system built on the **ESP32C3-Pro Development Board with OLED**. 

This project goes beyond a simple Pomodoro timer. It features a scalable, app-based architecture with smooth animations, custom localized typography, and an intuitive single-dial interface. It operates with **no physical power switch**, instead employing a **virtual power switch** implemented in software via ESP32-C3 Deep Sleep.

---

## 📱 TimOS Apps & Features

The device boots into a home launcher and allows access to multiple dedicated applications.

*   🏠 **Home Launcher:** Displays a real-time clock and date in both English and custom-rendered Bangla typography. Click to open the animated app carousel.
*   🍅 **App 1: Pomodoro Timer:** Advanced focus timer with a vertically animated selection menu. Features preset durations, an adjustable custom timer, an inverted progress bar, and session completion reports.
*   ⏰ **App 2: Alarm:** A standalone daily alarm utilizing the ESP32's internal real-time clock.
*   ⚙️ **App 3: Settings:** Manage device preferences including Date/Time, Buzzer Volume, and Screen Brightness.

### 🕹️ Global Interaction Rules
The entire OS is navigated using a single KY-040 Rotary Encoder:
*   **Dial Rotate:** Scroll through menus, adjust times/values, or carousel through apps.
*   **Single Click:** Select an option, enter an app, or move to the next input field (e.g., Hours -> Minutes).
*   **Long Press (800ms):** Universal **BACK** button. Cancel actions or exit apps back to the launcher.
*   **Extra Long Press (2s):** Universal **POWER** button. Safely shuts down the OS into Deep Sleep from any screen.

---

## 🔌 Hardware Connections (Wiring Pinout)

Connect the hardware peripherals as follows:

| Component | Pin | ESP32-C3 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **KY-040 Encoder** | CLK | **GPIO 0** | Rotary input clock |
| | DT | **GPIO 1** | Rotary input data |
| | SW | **GPIO 3** | Push-button click & wakeup |
| | VCC | **3.3V (3V)** | Power (Do NOT connect to 5V) |
| | GND | **GND** | Ground |
| **Buzzer** | Signal (+) | **GPIO 10** | Audio signal |
| | GND (-) | **GND** | Ground |
| **Battery Divider** | Midpoint | **GPIO 4** | Node between two $100\text{k}\Omega$ resistors |
| | VCC Input | **TP4056 OUT+** | Direct battery voltage |
| | GND | **GND** | Ground |

---

## ⚡ Power Selector & Charging Paths
We utilize a **1N4007 Diode** in a switchless configuration to handle both battery output and safe single-port charging via the ESP32 USB port:
*   Connect the **1N4007 Diode Anode** (unmarked side) to **TP4056 OUT+**.
*   Connect the **1N4007 Diode Cathode** (striped side) to the **ESP32 5V pin**.
*   Connect the **ESP32 5V pin** directly to **TP4056 IN+**.
*   Connect all **GND** pins together.

---

## ⚠️ Battery Safety & Low Power Modifications

For a safe and long-lasting battery-powered build, you **must** perform the following hardware modifications:

1.  **Restrict Charger Current ($R_{prog}$):**
    *   The TP4056 charger board defaults to a 1A charging current, which will damage or destroy a 150mAh battery.
    *   **Action:** Replace the $R_{prog}$ resistor (usually labeled R3 on the board, next to pin 2 of the TP4056 chip) with a **10 kΩ resistor**. This limits the charge current to a safe **~130mA**.
2.  **Eliminate TP4056 Leakage Current:**
    *   **Action:** Scratch off or desolder the **Blue LED** on the TP4056 board.
    *   **Why:** When running on battery power, the battery voltage back-feeds into the charger board, causing the Blue LED to stay illuminated and drain the battery in 4 days. Removing it blocks this path. (The **Red LED** is left intact and will still light up when charging).
3.  **Eliminate ESP32 Standby Current:**
    *   **Action:** Scratch off or desolder the onboard **PWR** (Power Indicator) LED on the ESP32-C3 board.
    *   **Why:** This LED is hardwired to the power rail and cannot be controlled in software. Removing it stops it from drawing ~2mA during deep sleep, extending standby battery life from 3 days to **over 6 months**.
