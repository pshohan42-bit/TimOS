# Hardware & Software Action Plan: TimOS

This document records the hardware configurations and the architectural blueprint for **TimOS**—a lightweight, scalable operating system designed for the ESP32-C3-Pro Pomodoro Timer.

---

## 🛠️ Completed Hardware Milestones

### 1. Safety Battery Charging Configuration
*   **Problem:** The TP4056 charger board defaults to a 1A charging rate, which would overheat and destroy the small 150mAh Li-ion battery.
*   **Completed Mod:** Swapped the default $R_{prog}$ resistor (R3, next to pin 2 of the TP4056 chip) with a **10 kΩ resistor**. This limits the maximum charging current to a safe, controlled **~130mA** ($0.86\text{C}$).

### 2. Switchless Power Diode Path
*   **Completed Mod:** Installed a **1N4007 Silicon Diode** between the battery charger output and the ESP32 board:
    *   **Anode** (no stripe) connected to **TP4056 OUT+**.
    *   **Cathode** (striped side) connected to **ESP32 5V pin**.
    *   **ESP32 5V pin** connected directly to **TP4056 IN+**.
*   **Outcome:** Allows the device to charge and program simultaneously through the single ESP32 USB-C port without needing a manual power switch.

### 3. Direct Battery Monitoring Divider
*   **Completed Mod:** Wired a $100\text{k}\Omega / 100\text{k}\Omega$ resistor voltage divider directly from **TP4056 OUT+** (Battery Positive) to Ground, with the midpoint connected to **GPIO 4 (ADC)**.
*   **Outcome:** Measures battery voltage directly at the source. This eliminates software inaccuracies caused by the diode voltage drop.

### 4. Standby Leakage Eliminations
*   **Completed Mod (TP4056):** Scratched off/removed the **Blue LED** on the TP4056 module. This blocks the battery from leaking current back through the status pin when unplugged, while keeping the Red LED active for charging feedback.
*   **Completed Mod (ESP32):** Scratched off/removed the hardwired **PWR LED** on the ESP32-C3 board to stop a constant 2mA drain during sleep.
*   **Outcome:** Reduces the idle standby current during Deep Sleep to the microamp level, extending standby battery life from 3 days to **over 6 months**.

---

## 📱 TimOS Architecture & Software Blueprint

The firmware is transitioning into a modular "Operating System" structure to support multiple applications, intuitive rotary-driven navigation, and scalability for future apps. 

### Global Interaction Rules
*   **Encoder Scroll:** Navigates menus, switches between apps on the launcher, or adjusts values in input fields.
*   **Button Push (Short):** Selects an app, confirms a setting, or moves to the next input field (e.g., from Hours to Minutes).
*   **Button Hold (Long - 800ms):** Universal "Back/Exit" button. Returns to the previous menu or exits an app back to the Launcher.
*   **Button Hold (Extra Long - 2s):** Universal System Shutdown (Enters Deep Sleep).

---

### Phase 1 Apps & Functionality

#### 1. Launcher (Home Screen)
*   **Visuals:** Displays the real-time clock and date using the ESP32's internal RTC.
*   **Localization:** Dual-language support displaying time/date in **English and Bangla**. *(Implementation note: Standard GFX libraries lack Bangla support. We will generate custom C-array bitmaps for Bangla digits and specific text elements to render them smoothly).*
*   **Action:** Pressing the button opens the App Menu.
*   **App Menu Overlay:** A horizontally scrolling carousel of apps. Features "old phone" style square icons (fallback to text if space constrained) with smooth side-to-side scrolling animations.

#### 2. App 1: Pomodoro Timer (Advanced)
*   **Menu:** Vertically animated scrolling list keeping the currently selected mode locked in the center of the screen.
*   **Modes:** Presets (Work, Short Break, Long Break) and a **new Custom Mode** allowing standard scroll/click time inputs.
*   **Active UI:** A large countdown and an **inverted progress bar** (starts completely empty and fills up to full as time progresses).
*   **Completion Screen:** A clean "Congratulations" screen asking to either start another timer or exit back to the launcher.

#### 3. App 2: RTC Alarm
*   **Function:** Utilizes the ESP32's internal real-time clock.
*   **Feature:** Single alarm implementation. 
*   **Input UI:** Uses the standardized Time Input UI. The user scrolls to set the Hour -> clicks -> scrolls to set the Minute -> clicks -> saves.

#### 4. App 3: Settings
*   **Time & Date:** Interface to manually set the internal RTC time and date without needing Wi-Fi sync.
*   **Volume:** Adjusts the buzzer PWM frequency/volume limits.
*   **Brightness:** Adjusts the OLED contrast register for screen brightness control.

---

## 📌 Wiring Pinout Reference

| Component | Pin Name | ESP32-C3 Pin | Status |
| :--- | :--- | :--- | :--- |
| **KY-040 Encoder** | CLK | **GPIO 0** | Verified |
| | DT | **GPIO 1** | Verified |
| | SW | **GPIO 3** | Verified (Wakeup source) |
| | VCC | **3.3V (3V)** | Verified |
| | GND | **GND** | Verified |
| **Buzzer** | Signal (+) | **GPIO 10** | Verified |
| | GND (-) | **GND** | Verified |
| **Battery Divider**| Midpoint | **GPIO 4** | Verified (Direct Bat) |
| **Status LED** | Onboard | **GPIO 7** | Verified (Power Indicator)|
| **OLED Screen** | SDA / SCL | **GPIO 5 / 6** | Verified (Un-mirrored) |
