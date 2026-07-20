# TimOS: ESP32C3-Pro Smart Timer Operating System

A premium, feature-rich, ultra-low-power standalone operating system built on the **ESP32C3-Pro Development Board with OLED**.

This project converts an ESP32-C3 microcontroller into a full-fledged smart desk companion. It features a scalable app-based architecture, smooth UI rendering, custom timekeeping, interactive games, and an intuitive single-dial interface. Operating with **no physical power switch**, it implements a **virtual power switch** using ESP32-C3 Deep Sleep and timer/GPIO wakeups.

---

## 📱 TimOS Apps & Features

TimOS boots into a Home Launcher and provides access to multiple dedicated applications:

*   🏠 **Home Launcher:** Displays real-time clock, calendar date, live battery percentage icon, and system status LED indicator. Press the encoder dial to open the app menu.
*   🍅 **Pomodoro App:** Focus timer featuring standard presets (25m Focus, 5m Short Break, 15m Long Break) and a Custom Time mode. Displays dynamic countdown timers, progress bars, pause/resume controls, and a completion screen with audio feedback.
*   ⏰ **Alarm App:** Standalone daily alarm powered by the ESP32 internal real-time clock (RTC). Features easy hour/minute setting, toggle enable/disable, ringing melody, and auto-wakeup from deep sleep when the alarm triggers.
*   ⚙️ **Settings App:** System customization menu to set Time & Date, toggle 12h/24h clock formats, select Buzzer Volume levels (`OFF`, `LOW`, `MED`, `HIGH`), and adjust OLED Brightness (`LOW`, `MED`, `HIGH`). All settings persist across power cycles via ESP32 Non-Volatile Storage (`Preferences` NVS).
*   🎮 **Tetris App:** Fully functional retro block puzzle game tailored for the 128x64 OLED display. Features piece spawning, rotation, horizontal movement, line clearing, score tracking, dynamic speed scaling, and game over screens.

---

## 🕹️ Global Interaction Rules

The entire OS is navigated using a single **KY-040 Rotary Encoder**:

*   **Dial Rotate:** Scroll through app lists, adjust numerical values (Hours, Minutes, Brightness, Volume), or move active blocks left/right in Tetris.
*   **Single Click:** Select a menu item, confirm entry, move to next input field, or rotate the falling block in Tetris.
*   **Long Press (800ms):** Universal **BACK / EXIT** button. Returns to the previous menu or exits an active app back to the Launcher.
*   **Extra Long Press (2s+):** Universal **POWER OFF** button. Shuts down the system into ESP32 Deep Sleep from any screen.
*   **Smart Auto-Sleep:** After 60 seconds of inactivity, the device automatically enters Deep Sleep to conserve battery (disabled while a Pomodoro timer is running or an alarm is active). Pressing the encoder button wakes the device up instantly.

---

## 🔌 Hardware Connections (Wiring Pinout)

Connect the hardware peripherals to the ESP32C3-Pro as follows:

| Component | Pin | ESP32-C3 Pin | Function / Description |
| :--- | :--- | :--- | :--- |
| **KY-040 Encoder** | CLK | **GPIO 0** | Rotary input clock line |
| | DT | **GPIO 1** | Rotary input data line |
| | SW | **GPIO 3** | Push-button input & Deep Sleep wakeup source |
| | VCC | **3.3V (3V)** | Logic power |
| | GND | **GND** | Ground |
| **Buzzer** | Signal (+) | **GPIO 10** | Piezo audio PWM output |
| | GND (-) | **GND** | Ground |
| **Status LED** | Onboard/Ext | **GPIO 7** | System activity indicator LED |
| **OLED Display** | SDA | **GPIO 5** | I2C Data (SSD1306 128x64) |
| | SCL | **GPIO 6** | I2C Clock (SSD1306 128x64) |
| **Battery Divider** | Midpoint | **GPIO 4** | ADC input from $100\text{k}\Omega / 100\text{k}\Omega$ voltage divider |
| | VCC Input | **TP4056 OUT+** | Direct lithium battery voltage input |
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

For a safe and long-lasting battery-powered build (using a 150mAh–500mAh Li-ion battery), perform the following hardware modifications:

1.  **Restrict Charger Current ($R_{prog}$):**
    *   The TP4056 charger board defaults to a 1A charging current, which will damage small Li-ion cells.
    *   **Action:** Replace the $R_{prog}$ resistor (R3, next to pin 2 of the TP4056 chip) with a **10 kΩ resistor**. This limits the charge current to a safe **~130mA**.
2.  **Eliminate TP4056 Leakage Current:**
    *   **Action:** Scratch off or desolder the **Blue LED** on the TP4056 board.
    *   **Why:** Prevents battery voltage from back-feeding into the charger status pin and draining the battery when unplugged. (The **Red LED** remains active for charging feedback).
3.  **Eliminate ESP32 Standby Current:**
    *   **Action:** Scratch off or desolder the hardwired **PWR** (Power Indicator) LED on the ESP32-C3 board.
    *   **Why:** Stops a constant ~2mA drain during deep sleep, extending standby battery life from 3 days to **over 6 months**.

---

## ⚙️ Building and Flashing

1. Open `TimOS/TimOS.ino` in the Arduino IDE or PlatformIO.
2. Select **ESP32C3 Dev Module** as the target board.
3. Install required libraries:
   * `Adafruit SSD1306`
   * `Adafruit GFX Library`
4. Set upload speed to `921600` baud and flash to your device.
