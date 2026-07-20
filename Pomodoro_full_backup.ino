/**
 * ESP32C3-Pro Pomodoro Timer (Backup of Full Code)
 * Features:
 *  - 0.96" OLED display UI with animated progress bar and battery monitor.
 *  - KY-040 Rotary Encoder for navigation, duration adjustments, and control.
 *  - Passive/Active Buzzer feedback (pleasant alerts, encoder tick sounds).
 *  - Onboard status LED (GPIO7) with custom breathing/flashing effects.
 *  - Power Saver: Automatic OLED blanking after 30 seconds of inactivity.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
#define ENABLE_BATTERY_MONITOR true
#define SLEEP_TIMEOUT 30000 // Sleep screen after 30 seconds of inactivity in menu

// States
enum TimerState { STATE_MENU, STATE_RUNNING, STATE_PAUSED, STATE_FINISHED };
TimerState currentState = STATE_MENU;

// Menu Modes
enum PomodoroMode { MODE_WORK, MODE_SHORT_BREAK, MODE_LONG_BREAK, MODE_CUSTOM, MODE_COUNT };
const char* modeNames[] = { "Work Session", "Short Break", "Long Break", "Custom Setup" };
int defaultMinutes[] = { 25, 5, 15, 25 }; // Custom defaults to 25 mins initially

int selectedMode = MODE_WORK;
int customMinutes = 25; // Editable duration for custom mode
int totalTimeSeconds = 0;
int timeLeftSeconds = 0;

// Timekeeping
unsigned long lastTickTime = 0;
unsigned long lastActivityTime = 0;
bool isScreenOn = true;

// Button Events
enum ButtonEvent { BUTTON_NONE, BUTTON_CLICK, BUTTON_LONG_PRESS };

void setup() {
  Serial.begin(115200);

  // Configure Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize I2C and OLED
  Wire.begin(5, 6); // SDA = GPIO5, SCL = GPIO6
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Show splash screen
  drawSplash();
  delay(1500);
  
  lastActivityTime = millis();
}

void loop() {
  // 1. Read Inputs
  int encoderDelta = readEncoder();
  ButtonEvent btnEvent = checkButton();

  // Reset activity timer on any physical input
  if (encoderDelta != 0 || btnEvent != BUTTON_NONE) {
    resetSleepTimer();
  }

  // 2. Handle State Machine
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

  // 3. Update LED Status Indicators (breathing, flashing)
  updateLED();

  // 4. Update Battery/Power Management
  handlePowerSaving();

  // 5. Draw UI (if screen is active)
  if (isScreenOn) {
    display.clearDisplay();
    drawBatteryIcon();
    
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
  
  delay(10); // Small cycle buffer
}

/* ========================================================================== */
/*                          INPUT HANDLERS & DEBOUNCE                         */
/* ========================================================================== */

int readEncoder() {
  static int lastClk = HIGH;
  int currentClk = digitalRead(ENCODER_CLK);
  int delta = 0;
  
  if (currentClk != lastClk) {
    lastClk = currentClk;
    if (currentClk == LOW) {
      if (digitalRead(ENCODER_DT) == HIGH) {
        delta = 1;
        playTick();
      } else {
        delta = -1;
        playTick();
      }
    }
  }
  return delta;
}

ButtonEvent checkButton() {
  static int lastButtonState = HIGH;
  static unsigned long buttonDownTime = 0;
  static bool isHolding = false;
  
  int reading = digitalRead(ENCODER_SW);
  ButtonEvent event = BUTTON_NONE;
  unsigned long now = millis();
  
  if (reading == LOW && lastButtonState == HIGH) {
    buttonDownTime = now;
    isHolding = true;
  } else if (reading == HIGH && lastButtonState == LOW) {
    if (isHolding) {
      if (now - buttonDownTime >= 800) {
        event = BUTTON_LONG_PRESS;
      } else if (now - buttonDownTime >= 50) {
        event = BUTTON_CLICK;
      }
      isHolding = false;
    }
  } else if (reading == LOW && isHolding) {
    if (now - buttonDownTime >= 800) {
      event = BUTTON_LONG_PRESS;
      isHolding = false; // Trigger once
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
    if (selectedMode == MODE_CUSTOM) {
      // In Custom Setup mode, rotating encoder adjusts time directly
      customMinutes += delta;
      if (customMinutes < 1) customMinutes = 1;
      if (customMinutes > 99) customMinutes = 99;
      defaultMinutes[MODE_CUSTOM] = customMinutes;
    } else {
      // Navigate menu selections
      selectedMode += delta;
      if (selectedMode < 0) selectedMode = MODE_COUNT - 1;
      if (selectedMode >= MODE_COUNT) selectedMode = 0;
    }
  }

  if (btn == BUTTON_CLICK) {
    if (selectedMode == MODE_CUSTOM && delta == 0) {
      // Double check: First click selects/enters Custom config, second starts
    }
    
    // Start session
    totalTimeSeconds = defaultMinutes[selectedMode] * 60;
    timeLeftSeconds = totalTimeSeconds;
    currentState = STATE_RUNNING;
    lastTickTime = millis();
    playNotificationTone();
  }
}

void handleRunningState(int delta, ButtonEvent btn) {
  unsigned long now = millis();
  
  // Timer tick down
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
    lastTickTime = millis(); // Refresh start baseline
    playSelectTone();
  }

  // Handle long press -> reset to menu
  if (btn == BUTTON_LONG_PRESS) {
    currentState = STATE_MENU;
    playCancelTone();
  }
}

void handleFinishedState(int delta, ButtonEvent btn) {
  // Any input returns to main menu
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
  // Pleasant ascending tune
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

void resetSleepTimer() {
  lastActivityTime = millis();
  if (!isScreenOn) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    isScreenOn = true;
  }
}

void handlePowerSaving() {
  // Only auto-sleep if we are in the main menu (not timing)
  if (currentState == STATE_MENU) {
    if (millis() - lastActivityTime > SLEEP_TIMEOUT) {
      if (isScreenOn) {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        isScreenOn = false;
        // Set LED to complete off to save current
        analogWrite(LED_PIN, 0);
      }
    }
  }
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

void drawBatteryIcon() {
  if (!ENABLE_BATTERY_MONITOR) return;

  // Read voltage
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 4095.0) * 3.3 * 2.0; // Divider factor of 2

  // Map to percentage (3.3V as cut-off, 4.2V as max)
  int pct = (int)((voltage - 3.3) / 0.9 * 100.0);
  pct = constrain(pct, 0, 100);

  // Draw battery frame
  display.drawRect(110, 2, 14, 7, SSD1306_WHITE);
  display.fillRect(124, 4, 2, 3, SSD1306_WHITE); // Tip

  // Fill index bar
  int fillWidth = map(pct, 0, 100, 0, 10);
  if (fillWidth > 0) {
    display.fillRect(112, 4, fillWidth, 3, SSD1306_WHITE);
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
      // Invert row colors for current selection
      display.fillRoundRect(2, yPos - 1, 104, 10, 2, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(6, yPos);
    display.print(modeNames[i]);

    // Draw the active time values
    display.setCursor(82, yPos);
    if (i == MODE_CUSTOM) {
      display.printf("%2dm", customMinutes);
    } else {
      display.printf("%2dm", defaultMinutes[i]);
    }
  }
  display.setTextColor(SSD1306_WHITE); // Reset text color
}

void drawCountdown(const char* stateLabel) {
  // Title / Mode label
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print(modeNames[selectedMode]);
  
  // Show active status (RUNNING/PAUSED)
  display.setCursor(68, 2);
  display.print(stateLabel);
  display.drawFastHLine(0, 12, 108, SSD1306_WHITE);

  // Large timer
  int mins = timeLeftSeconds / 60;
  int secs = timeLeftSeconds % 60;
  display.setTextSize(3);
  display.setCursor(20, 22);
  display.printf("%02d:%02d", mins, secs);

  // Time progress bar
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
/*                            STATUS LED BREEDING                             */
/* ========================================================================== */

void updateLED() {
  if (!isScreenOn && currentState == STATE_MENU) {
    analogWrite(LED_PIN, 0); // Completely off in screen-saver mode
    return;
  }

  switch (currentState) {
    case STATE_RUNNING: {
      float val = (sin(millis() / 1500.0 * PI) + 1.0) / 2.0; // 0.0 to 1.0
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
