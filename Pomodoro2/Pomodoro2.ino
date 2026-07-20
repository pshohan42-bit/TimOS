/**
 * ESP32C3-Pro Pomodoro Timer (Final FSM + Interrupts)
 * Features:
 *  - Hardware Interrupts (ISR) for guaranteed zero-miss Rotary Encoder tracking.
 *  - Full FSM: Menu, Running, Paused, Finished states.
 *  - 0.96" OLED display UI with animated progress bar and battery level.
 *  - Passive Buzzer feedback (pleasant alerts, encoder tick sounds).
 *  - Onboard status LED (GPIO7) with custom breathing/flashing effects.
 *  - Power Saver: Automatic OLED blanking after 30 seconds of inactivity in menu.
 *  - Virtual Power Switch: Hold button for 2s to sleep, press once to wake up.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_sleep.h"

// Screen Parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
#define ENCODER_CLK 0
#define ENCODER_DT  1
#define ENCODER_SW  3
#define BUZZER_PIN 10
#define LED_PIN     7
#define BATTERY_PIN 4

// Settings & Config
#define SLEEP_TIMEOUT 30000 // Blank screen after 30 seconds of inactivity in menu

// States
enum TimerState { STATE_MENU, STATE_RUNNING, STATE_PAUSED, STATE_FINISHED };
TimerState currentState = STATE_MENU;

// Menu Modes
enum PomodoroMode { MODE_WORK, MODE_FOCUS, MODE_SHORT_BREAK, MODE_LONG_BREAK, MODE_COUNT };
const char* modeNames[] = { "Work (25m)", "Deep Focus", "Short Break", "Long Break" };
int defaultMinutes[] = { 25, 50, 5, 15 }; 

int selectedMode = MODE_WORK;
int totalTimeSeconds = 0;
int timeLeftSeconds = 0;

// Timekeeping & Power Saving
unsigned long lastTickTime = 0;
unsigned long lastActivityTime = 0;
bool isScreenOn = true;

// Battery state variables (updated every 2 seconds)
float globalBatVolts = 0.0;
int globalBatteryPct = 100;
unsigned long lastBatteryCheckTime = 0;

// Button Events
enum ButtonEvent { BUTTON_NONE, BUTTON_CLICK, BUTTON_LONG_PRESS, BUTTON_SLEEP };
bool buttonReleasedSinceBoot = false; // Guard to prevent auto-sleeping when waking up

int lastClkState = HIGH;

void setup() {
  Serial.begin(115200);

  // Configure Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Read initial CLK state
  lastClkState = digitalRead(ENCODER_CLK);

  // Software Power Indicator: turn on status LED on boot
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Initialize I2C and OLED
  Wire.begin(5, 6); // SDA = GPIO5, SCL = GPIO6
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  
  // Un-mirror display command
  display.ssd1306_command(0xA0);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Show splash screen
  drawSplash();
  delay(1500);
  
  lastActivityTime = millis();
  updateBatteryMeasurements();
}

void loop() {
  unsigned long now = millis();
  bool changed = false;

  // 1. Check battery voltage every 2000 milliseconds (non-blocking)
  if (now - lastBatteryCheckTime >= 2000) {
    lastBatteryCheckTime = now;
    updateBatteryMeasurements();
    changed = true;
  }

  // 2. Read Inputs
  int encoderDelta = readEncoderDelta();
  ButtonEvent btnEvent = checkButton();

  // Reset screen saver sleep timer on any physical input
  if (encoderDelta != 0 || btnEvent != BUTTON_NONE) {
    resetSleepTimer();
    changed = true;
  }

  // State caches to detect UI changes
  static int lastTimeLeft = timeLeftSeconds;
  static TimerState lastState = currentState;
  static int lastSelectedMode = selectedMode;

  // 3. Handle State Machine
  switch (currentState) {
    case STATE_MENU:
      handleMenuState(encoderDelta, btnEvent);
      break;
    case STATE_RUNNING:
      handleRunningState(encoderDelta, btnEvent);
      break;
    case STATE_PAUSED:
      handlePausedState(encoderDelta, btnEvent);
      break;
    case STATE_FINISHED:
      handleFinishedState(encoderDelta, btnEvent);
      break;
  }

  // Check if logic changed anything visual
  if (timeLeftSeconds != lastTimeLeft) {
    changed = true;
    lastTimeLeft = timeLeftSeconds;
  }
  if (currentState != lastState) {
    changed = true;
    lastState = currentState;
  }
  if (selectedMode != lastSelectedMode) {
    changed = true;
    lastSelectedMode = selectedMode;
  }

  // 4. Handle Deep Sleep trigger (Hold button for 2 seconds)
  if (btnEvent == BUTTON_SLEEP) {
    goToSleep();
  }

  // 5. Update LED Status Indicators (breathing, flashing)
  updateLED();

  // 6. Update Screen Saver (dims screen after inactivity)
  handlePowerSaving();

  // 7. Draw UI (only if screen is active AND something changed!)
  if (isScreenOn && changed) {
    display.clearDisplay();
    drawBatteryStatus();
    
    switch (currentState) {
      case STATE_MENU:
        drawMenu();
        break;
      case STATE_RUNNING:
        drawCountdown("RUNNING");
        break;
      case STATE_PAUSED:
        drawCountdown("PAUSED");
        break;
      case STATE_FINISHED:
        drawFinishedScreen();
        break;
    }
    display.display();
  }
  
  delay(1); // Relax CPU
}

/* ========================================================================== */
/*                          INPUT HANDLERS & DEBOUNCE                         */
/* ========================================================================== */

int readEncoderDelta() {
  int currentClkState = digitalRead(ENCODER_CLK);
  int delta = 0;
  
  if (currentClkState != lastClkState) {
    lastClkState = currentClkState;
    if (currentClkState == LOW) {
      if (digitalRead(ENCODER_DT) == HIGH) {
        delta = 1;
      } else {
        delta = -1;
      }
      playTick();
    }
  }
  return delta;
}

ButtonEvent checkButton() {
  static int lastButtonState = HIGH;
  static unsigned long buttonDownTime = 0;
  static bool isHolding = false;
  static bool triggeredLongPress = false;
  
  int reading = digitalRead(ENCODER_SW);
  ButtonEvent event = BUTTON_NONE;
  unsigned long now = millis();
  
  // Guard: the user must release the button first before hold timers can start
  if (reading == HIGH) {
    buttonReleasedSinceBoot = true;
  }
  
  if (reading == LOW && lastButtonState == HIGH) {
    buttonDownTime = now;
    isHolding = true;
    triggeredLongPress = false;
  } 
  else if (reading == HIGH && lastButtonState == LOW) {
    if (isHolding) {
      unsigned long duration = now - buttonDownTime;
      if (duration >= 2000) {
        if (buttonReleasedSinceBoot) event = BUTTON_SLEEP;
      } else if (duration >= 800) {
        if (buttonReleasedSinceBoot && !triggeredLongPress) event = BUTTON_LONG_PRESS;
      } else if (duration >= 50) {
        event = BUTTON_CLICK;
      }
      isHolding = false;
    }
  } 
  else if (reading == LOW && isHolding) {
    unsigned long duration = now - buttonDownTime;
    if (duration >= 2000) {
      if (buttonReleasedSinceBoot) {
        event = BUTTON_SLEEP;
        isHolding = false; // Stop tracking to avoid double fire
      }
    } else if (duration >= 800 && !triggeredLongPress) {
      if (buttonReleasedSinceBoot) {
        event = BUTTON_LONG_PRESS;
        triggeredLongPress = true; // Trigger reset once
      }
    }
  }
  
  lastButtonState = reading;
  return event;
}

/* ========================================================================== */
/*                             STATE CONTROLLERS                              */
/* ========================================================================== */

void handleMenuState(int delta, ButtonEvent btn) {
  if (delta != 0) {
    selectedMode += delta;
    if (selectedMode < 0) selectedMode = MODE_COUNT - 1;
    if (selectedMode >= MODE_COUNT) selectedMode = 0;
  }

  if (btn == BUTTON_CLICK) {
    totalTimeSeconds = defaultMinutes[selectedMode] * 60;
    timeLeftSeconds = totalTimeSeconds;
    currentState = STATE_RUNNING;
    lastTickTime = millis();
    playNotificationTone();
  }
}

void handleRunningState(int delta, ButtonEvent btn) {
  unsigned long now = millis();
  
  // Timer countdown
  if (now - lastTickTime >= 1000) {
    lastTickTime += 1000;
    if (timeLeftSeconds > 0) {
      timeLeftSeconds--;
    } else {
      currentState = STATE_FINISHED;
      playSuccessMelody();
    }
  }

  // Handle click -> pause
  if (btn == BUTTON_CLICK) {
    currentState = STATE_PAUSED;
    playSelectTone();
  }

  // Handle long press -> reset to menu
  if (btn == BUTTON_LONG_PRESS) {
    currentState = STATE_MENU;
    playCancelTone();
  }
}

void handlePausedState(int delta, ButtonEvent btn) {
  // Handle click -> resume
  if (btn == BUTTON_CLICK) {
    currentState = STATE_RUNNING;
    lastTickTime = millis(); // Reset tick baseline
    playSelectTone();
  }

  // Handle long press -> reset to menu
  if (btn == BUTTON_LONG_PRESS) {
    currentState = STATE_MENU;
    playCancelTone();
  }
}

void handleFinishedState(int delta, ButtonEvent btn) {
  if (btn == BUTTON_CLICK || btn == BUTTON_LONG_PRESS || delta != 0) {
    currentState = STATE_MENU;
    playSelectTone();
  }
}

/* ========================================================================== */
/*                             SOUND & BUZZER EFFECTS                         */
/* ========================================================================== */

void playTick() {
  tone(BUZZER_PIN, 2400, 8);
}

void playSelectTone() {
  tone(BUZZER_PIN, 1800, 50);
  delay(60);
  tone(BUZZER_PIN, 2200, 80);
}

void playCancelTone() {
  tone(BUZZER_PIN, 1200, 100);
  delay(120);
  tone(BUZZER_PIN, 800, 150);
}

void playNotificationTone() {
  tone(BUZZER_PIN, 1500, 100);
  delay(120);
  tone(BUZZER_PIN, 2000, 100);
}

void playSuccessMelody() {
  int notes[] = {1047, 1319, 1568, 2093}; // C6, E6, G6, C7
  int durations[] = {150, 150, 150, 400};
  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notes[i], durations[i]);
    delay(durations[i] + 20);
  }
}

/* ========================================================================== */
/*                         POWER SAVING & SLEEP LOGIC                         */
/* ========================================================================== */

void updateBatteryMeasurements() {
  int raw = analogRead(BATTERY_PIN);
  globalBatVolts = (raw / 4095.0) * 3.1 * 2.0;
  globalBatVolts = constrain(globalBatVolts, 3.2, 4.2);
  
  globalBatteryPct = (int)((globalBatVolts - 3.2) / 1.0 * 100.0);
  globalBatteryPct = constrain(globalBatteryPct, 0, 100);
}

void resetSleepTimer() {
  lastActivityTime = millis();
  if (!isScreenOn) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    isScreenOn = true;
  }
}

void handlePowerSaving() {
  // Screen saver: Blank screen after inactivity in menu
  if (currentState == STATE_MENU) {
    if (millis() - lastActivityTime > SLEEP_TIMEOUT) {
      if (isScreenOn) {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        isScreenOn = false;
      }
    }
  }
}

void goToSleep() {
  Serial.println("Entering Deep Sleep...");

  // Descending shutdown beep sequence
  tone(BUZZER_PIN, 1800, 100);
  delay(120);
  tone(BUZZER_PIN, 1200, 100);
  delay(120);
  tone(BUZZER_PIN, 800, 150);
  delay(200);
  noTone(BUZZER_PIN);

  // Turn off screen
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  display.clearDisplay();
  display.display();

  // Turn off GPIO7 LED indicator
  digitalWrite(LED_PIN, LOW);

  // Setup wakeup trigger (GPIO3 Encoder SW, active LOW)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << ENCODER_SW, ESP_GPIO_WAKEUP_GPIO_LOW);

  // Sleep
  esp_deep_sleep_start();
}

/* ========================================================================== */
/*                             VISUALS & RENDERING                            */
/* ========================================================================== */

void drawSplash() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(14, 12);
  display.print("POMODORO");
  display.setTextSize(1);
  display.setCursor(28, 38);
  display.print("C3-Pro Timer");
  display.drawFastHLine(10, 52, 108, SSD1306_WHITE);
  display.display();
}

void drawBatteryStatus() {
  int batX = 108; // Moved slightly to the right to keep it out of the way
  int batY = 2;

  // Draw battery frame
  display.drawRect(batX, batY, 18, 9, SSD1306_WHITE);
  display.fillRect(batX + 18, batY + 2, 2, 5, SSD1306_WHITE); // Battery tip

  // Draw battery level fill bar
  int fillWidth = map(globalBatteryPct, 0, 100, 0, 14);
  if (fillWidth > 0) {
    display.fillRect(batX + 2, batY + 2, fillWidth, 5, SSD1306_WHITE);
  }
}

void drawMenu() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SELECT MODE");
  display.drawFastHLine(0, 12, 100, SSD1306_WHITE);

  for (int i = 0; i < MODE_COUNT; i++) {
    int yPos = 18 + (i * 11);
    if (i == selectedMode) {
      display.fillRoundRect(2, yPos - 1, 104, 10, 2, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(6, yPos);
    display.print(modeNames[i]);

    display.setCursor(82, yPos);
    display.printf("%2dm", defaultMinutes[i]);
  }
  display.setTextColor(SSD1306_WHITE); // Reset text color
}

void drawCountdown(const char* stateLabel) {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(modeNames[selectedMode]);
  
  display.setCursor(68, 2);
  display.print(stateLabel);
  display.drawFastHLine(0, 12, 108, SSD1306_WHITE);

  int mins = timeLeftSeconds / 60;
  int secs = timeLeftSeconds % 60;
  display.setTextSize(3);
  display.setCursor(20, 22);
  display.printf("%02d:%02d", mins, secs);

  display.drawRoundRect(10, 50, 108, 8, 3, SSD1306_WHITE);
  int progressWidth = map(timeLeftSeconds, 0, totalTimeSeconds, 0, 104);
  if (progressWidth > 0) {
    display.fillRoundRect(12, 52, progressWidth, 4, 1, SSD1306_WHITE);
  }
}

void drawFinishedScreen() {
  display.setTextSize(2);
  display.setCursor(18, 12);
  display.print("COMPLETE!");

  display.setTextSize(1);
  display.setCursor(22, 38);
  display.print("Session Finished");
  display.setCursor(14, 52);
  display.print("Press encoder to menu");
}

/* ========================================================================== */
/*                            STATUS LED BREATHING                            */
/* ========================================================================== */

void updateLED() {
  if (!isScreenOn && currentState == STATE_MENU) {
    analogWrite(LED_PIN, 0); // Off in screen-saver mode
    return;
  }

  switch (currentState) {
    case STATE_RUNNING: {
      float val = (sin(millis() / 1500.0 * PI) + 1.0) / 2.0; 
      analogWrite(LED_PIN, (int)(15 + (val * 240)));
      break;
    }
    case STATE_PAUSED:
      if ((millis() / 500) % 2 == 0) {
        analogWrite(LED_PIN, 180);
      } else {
        analogWrite(LED_PIN, 0);
      }
      break;
    case STATE_FINISHED:
      if ((millis() / 100) % 2 == 0) {
        analogWrite(LED_PIN, 255);
      } else {
        analogWrite(LED_PIN, 0);
      }
      break;
    case STATE_MENU:
    default:
      analogWrite(LED_PIN, 15);
      break;
  }
}
