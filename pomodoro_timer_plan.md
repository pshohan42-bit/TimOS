# Hardware & Software Action Plan: TimOS

This document records the hardware configurations, architectural blueprint, and development status for **TimOS**—a lightweight, scalable operating system designed for the ESP32-C3-Pro Smart Timer & Desk Companion.

---

## 🛠️ Completed Hardware Milestones

### 1. Safety Battery Charging Configuration
*   **Problem:** The TP4056 charger board defaults to a 1A charging rate, which would overheat and destroy small (150mAh–500mAh) Li-ion batteries.
*   **Completed Mod:** Swapped the default $R_{prog}$ resistor (R3, next to pin 2 of the TP4056 chip) with a **10 kΩ resistor**. This limits the maximum charging current to a safe, controlled **~130mA** ($0.86\text{C}$).

### 2. Switchless Power Diode Path
*   **Completed Mod:** Installed a **1N4007 Silicon Diode** between the battery charger output and the ESP32 board:
    *   **Anode** (no stripe) connected to **TP4056 OUT+**.
    *   **Cathode** (striped side) connected to **ESP32 5V pin**.
    *   **ESP32 5V pin** connected directly to **TP4056 IN+**.
*   **Outcome:** Allows the device to charge and program simultaneously through the single ESP32 USB-C port without needing a manual power switch.

### 3. Direct Battery Monitoring Divider
*   **Completed Mod:** Wired a $100\text{k}\Omega / 100\text{k}\Omega$ resistor voltage divider directly from **TP4056 OUT+** (Battery Positive) to Ground, with the midpoint connected to **GPIO 4 (ADC)**.
*   **Outcome:** Measures battery voltage directly at the source, eliminating software inaccuracies caused by diode voltage drop. Calculates live voltage ($V_{bat}$) and percentage estimates ($0-100\%$).

### 4. Standby Leakage Eliminations
*   **Completed Mod (TP4056):** Scratched off/removed the **Blue LED** on the TP4056 module. This blocks current back-feeding when unplugged while keeping the Red LED active for charging status.
*   **Completed Mod (ESP32):** Scratched off/removed the hardwired **PWR LED** on the ESP32-C3 board to stop a constant 2mA drain during deep sleep.
*   **Outcome:** Reduces the idle standby current during Deep Sleep to microamp levels, extending standby battery life from 3 days to **over 6 months**.

---

## 📱 TimOS Architecture & Implementation Status

### Global Interaction Rules
*   **Encoder Scroll:** Navigates menus, switches apps on the launcher, adjusts numerical values, or moves Tetris blocks.
*   **Button Push (Short):** Selects an app, confirms settings, moves to next input field, or rotates Tetris pieces.
*   **Button Hold (Long - 800ms):** Universal "Back/Exit" button. Returns to the previous menu or exits an app back to the Launcher.
*   **Button Hold (Extra Long - 2s+):** Universal System Shutdown (enters ESP32 Deep Sleep).
*   **Auto-Sleep (60s Idle):** Automatically enters Deep Sleep after 60s of non-interaction when idle (inhibited during active Pomodoro or Alarm ringing).

---

### Implemented Apps & Functionality

#### 1. Launcher (Home Screen)
*   **Visuals:** Real-time clock and calendar date display via internal ESP32 RTC.
*   **Battery Status:** OLED top bar battery icon showing percentage and status LED feedback.
*   **Action:** Clicking encoder button opens the App Menu carousel.

#### 2. App 1: Pomodoro Timer
*   **Modes:** Presets (25m Focus, 5m Short Break, 15m Long Break) and Custom Time configuration.
*   **Active UI:** Countdown timer display, progress bar, pause/resume state management.
*   **Completion Screen:** "Finished" screen with options to restart timer or exit back to launcher, accompanied by buzzer melodies.

#### 3. App 2: RTC Alarm
*   **Feature:** Daily alarm with time input UI (Hours & Minutes) and toggle enable/disable.
*   **Deep Sleep Integration:** Calculates seconds until alarm time and sets ESP32 timer wakeup source (`esp_sleep_enable_timer_wakeup`) to automatically power on the device when the alarm fires.
*   **Ringing UI:** Screen notification and ringing melody.

#### 4. App 3: Settings
*   **Time & Date:** Interfaces to set real-time clock HH:MM and date DD/MM/YYYY.
*   **Clock Format:** Toggle between 12-hour and 24-hour display mode.
*   **Volume Control:** Buzzer levels (`OFF`, `LOW`, `MED`, `HIGH`).
*   **Brightness Control:** OLED contrast register control (`LOW`, `MED`, `HIGH`).
*   **NVS Storage:** All settings are stored non-volatilely using ESP32 `Preferences`.

#### 5. App 4: Tetris Game
*   **Gameplay:** Retro block puzzle game adapted for 128x64 OLED.
*   **Mechanics:** Matrix grid (20x10), collision detection, piece rotation (7 Tetromino types), line removal, score multiplier, speed progression, and Game Over screen.

---

## 📌 Wiring Pinout Reference

| Component | Pin Name | ESP32-C3 Pin | Status |
| :--- | :--- | :--- | :--- |
| **KY-040 Encoder** | CLK | **GPIO 0** | Verified |
| | DT | **GPIO 1** | Verified |
| | SW | **GPIO 3** | Verified (Deep Sleep Wakeup source) |
| | VCC | **3.3V (3V)** | Verified |
| | GND | **GND** | Verified |
| **Buzzer** | Signal (+) | **GPIO 10** | Verified (PWM Audio) |
| | GND (-) | **GND** | Verified |
| **Status LED** | Onboard/Ext | **GPIO 7** | Verified (Power/Activity LED)|
| **OLED Screen** | SDA / SCL | **GPIO 5 / 6** | Verified (SSD1306 I2C) |
| **Battery Divider**| Midpoint | **GPIO 4** | Verified (ADC $100\text{k}\Omega / 100\text{k}\Omega$) |
