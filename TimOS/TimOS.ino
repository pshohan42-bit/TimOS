/**
 * TimOS: ESP32C3-Pro Smart Timer Operating System
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_sleep.h"
#include <sys/time.h>
#include <Preferences.h>

Preferences preferences;

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

// Global OS States
enum OSState {
  OS_LAUNCHER,
  OS_APP_MENU,
  OS_APP_POMODORO,
  OS_APP_ALARM,
  OS_APP_SETTINGS,
  OS_APP_TETRIS,
  OS_APP_READER
};
OSState currentOSState = OS_LAUNCHER;

// App Menu Config
#define NUM_APPS 5
const char* appNames[NUM_APPS] = {"Pomodoro", "Alarm", "Settings", "Tetris", "Reader"};
int selectedAppIndex = 0;

// Pomodoro Variables
enum PomoState {
  POMO_STATE_MENU,
  POMO_STATE_CUSTOM_MINS,
  POMO_STATE_CUSTOM_SECS,
  POMO_STATE_RUNNING,
  POMO_STATE_PAUSED,
  POMO_STATE_FINISHED
};
PomoState currentPomoState = POMO_STATE_MENU;

const char* pomoModes[4] = {"25 Min Focus", "5 Min Break", "15 Min Break", "Custom Time"};
int selectedPomoMode = 0;
int customMinutes = 25;
int customSeconds = 0;
int pomoTotalSeconds = 0;
int pomoLeftSeconds = 0;
int pomoFinishedSelect = 0; // 0 = Restart, 1 = Exit

// Alarm Variables
bool alarmEnabled = false;
int alarmHour = 6;
int alarmMinute = 0;
bool alarmActive = false; // Is it currently ringing?

enum AlarmState {
  ALARM_STATE_MENU,
  ALARM_STATE_SET_HOUR,
  ALARM_STATE_SET_MINUTE,
  ALARM_STATE_RINGING
};
AlarmState currentAlarmState = ALARM_STATE_MENU;
int alarmMenuSelect = 0; // 0 = Toggle status, 1 = Set Time
int tempAlarmHour = 6;
int tempAlarmMinute = 0;

// Settings Variables
enum SettingsState {
  SET_STATE_MENU,
  SET_STATE_TIME_HH,
  SET_STATE_TIME_MM,
  SET_STATE_DATE_DD,
  SET_STATE_DATE_MM,
  SET_STATE_DATE_YY,
  SET_STATE_VOLUME,
  SET_STATE_BRIGHTNESS
};
SettingsState currentSettingsState = SET_STATE_MENU;
int settingsMenuSelect = 0; // 0=Time, 1=Date, 2=Time Format, 3=Volume, 4=Brightness
int tempHour = 12;
int tempMinute = 0;
int tempDay = 1;
int tempMonth = 1;
int tempYear = 2026;
int systemVolume = 2;       // 0=OFF, 1=LOW, 2=MED, 3=HIGH
int systemBrightness = 2;   // 0=LOW, 1=MED, 2=HIGH
bool use12HourFormat = false;

// Tetris Variables
#define TETRIS_ROWS 20
#define TETRIS_COLS 10
#define BLOCK_SIZE  5
#define TETRIS_X_OFFSET 7

uint16_t tetrisBoard[TETRIS_ROWS];
enum TetrisState { TETRIS_MENU, TETRIS_PLAYING, TETRIS_GAMEOVER };
TetrisState currentTetrisState = TETRIS_MENU;
int tetrisScore = 0;
int tetrisSpeed = 500;
unsigned long lastTetrisDrop = 0;
int currentPieceType = 0;
int currentPieceRot = 0;
int currentPieceX = 3;
int currentPieceY = 0;

const uint16_t tetrominoes[7][4] = {
  { 0x0F00, 0x2222, 0x00F0, 0x4444 }, // I
  { 0x44C0, 0x8E00, 0x6440, 0x0E20 }, // J
  { 0x4460, 0x0E80, 0xC440, 0x2E00 }, // L
  { 0xCC00, 0xCC00, 0xCC00, 0xCC00 }, // O
  { 0x06C0, 0x8C40, 0x6C00, 0x4620 }, // S
  { 0x0E40, 0x4C40, 0x4E00, 0x4640 }, // T
  { 0x0C60, 0x4C80, 0xC600, 0x2640 }  // Z
};

// Reader Variables
const char* sampleReaderText = "TimOS is a powerful smart timer operating system built for the ESP32C3-Pro. It features a complete RSVP speed reading engine. This allows you to read text rapidly without moving your eyes! The current focus word is highlighted, while the previous and next context words are faded gracefully. Try adjusting the WPM speed using the rotary dial on the fly. Happy reading!";
enum ReaderState { READER_BOOK_SELECT, READER_CHAPTER_SELECT, READER_MENU, READER_PLAYING, READER_PAUSED, READER_FINISHED };
ReaderState currentReaderState = READER_BOOK_SELECT;
int readerWpm = 250;
int readerWordIndex = 0;
int readerTotalWords = 0;
unsigned long lastReaderWordTime = 0;
#define MAX_READER_WORDS 100
String readerWords[MAX_READER_WORDS];

// Battery
float globalBatVolts = 0.0;
int globalBatteryPct = 100;
unsigned long lastBatteryCheckTime = 0;

// Encoder & Button
int lastClkState = HIGH;
enum ButtonEvent { BUTTON_NONE, BUTTON_CLICK, BUTTON_LONG_PRESS, BUTTON_HOME_PRESS, BUTTON_SLEEP };
bool buttonReleasedSinceBoot = false;
unsigned long lastActivityTime = 0;

// Forward Declarations
void setDisplayBrightness(int level);
void updateBatteryMeasurements();
int readEncoderDelta();
ButtonEvent checkButton();
void goToSleep();
void playTick();
void playSelectTone();
void playCancelTone();
void playFinishedMelody();
void updateLED();
void drawBatteryStatus();
void drawLauncher();
void drawAppMenu();
void drawPomodoroApp();
void drawAlarmApp();
void drawSettingsApp();
void drawTetrisApp();
void drawReaderApp();
void handleAlarmApp(int encoderDelta, ButtonEvent btnEvent);
void handleSettingsApp(int encoderDelta, ButtonEvent btnEvent);
void handleTetrisApp(int encoderDelta, ButtonEvent btnEvent);
void handleReaderApp(int encoderDelta, ButtonEvent btnEvent);
bool checkTetrisCollision(int pType, int pRot, int pX, int pY);
void lockTetrisPiece();
void spawnTetrisPiece();


void drawAppMenu() {
  // Top Title Bar
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("APPS");
  display.drawFastHLine(0, 12, 100, SSD1306_WHITE);

  int centerX = 64;
  int iconSpacing = 48; // Spacing to show left/center/right icons comfortably
  int boxSize = 28;     // Compact box size (28x28) to stay clear of bottom text
  int boxY = 16;
  
  // Circular App Indices: [Left, Center, Right]
  int appIndices[3] = {
    (selectedAppIndex - 1 + NUM_APPS) % NUM_APPS,
    selectedAppIndex,
    (selectedAppIndex + 1) % NUM_APPS
  };

  int xPositions[3] = {
    centerX - iconSpacing,
    centerX,
    centerX + iconSpacing
  };

  for (int slot = 0; slot < 3; slot++) {
    int appIdx = appIndices[slot];
    int iconX = xPositions[slot];
    bool isSelected = (slot == 1); // Center slot is selected
    
    // Draw Icon Box
    if (isSelected) {
      display.fillRoundRect(iconX - (boxSize/2), boxY, boxSize, boxSize, 4, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRoundRect(iconX - (boxSize/2), boxY, boxSize, boxSize, 4, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
    
    // Draw custom vector icon shapes
    if (appIdx == 0) { // Pomodoro: Circle timer
       display.drawCircle(iconX, boxY + 14, 7, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
       display.drawLine(iconX, boxY + 14, iconX, boxY + 8, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
    } else if (appIdx == 1) { // Alarm: Bell triangle
       display.fillTriangle(iconX, boxY + 6, iconX - 7, boxY + 18, iconX + 7, boxY + 18, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
    } else if (appIdx == 2) { // Settings: Gear/Sliders
       display.drawRect(iconX - 5, boxY + 9, 10, 10, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
       display.drawRect(iconX - 2, boxY + 5, 4, 18, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
       display.drawRect(iconX - 8, boxY + 12, 16, 4, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
    } else if (appIdx == 3) { // Tetris: T-Block shape
       display.fillRect(iconX - 6, boxY + 6, 12, 4, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
       display.fillRect(iconX - 2, boxY + 10, 4, 4, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
    }
    
    // Only display name of the selected app in the center to prevent overlaps and bottom-edge clipping
    if (isSelected) {
      display.setTextColor(SSD1306_WHITE);
      int textW = strlen(appNames[appIdx]) * 6;
      display.setCursor(iconX - (textW/2), boxY + boxSize + 6); // Renders safely at Y=50
      display.print(appNames[appIdx]);
    }
  }
}

void drawBatteryStatus() {
  int batX = 108;
  int batY = 2;
  // Frame
  display.drawRect(batX, batY, 18, 9, SSD1306_WHITE);
  display.fillRect(batX + 18, batY + 2, 2, 5, SSD1306_WHITE); // Tip
  // Fill
  int fillWidth = map(globalBatteryPct, 0, 100, 0, 14);
  if (fillWidth > 0) {
    display.fillRect(batX + 2, batY + 2, fillWidth, 5, SSD1306_WHITE);
  }
}

/* ========================================================================== */
/*                             POMODORO APPLICATION                           */
/* ========================================================================== */

void drawPomoMenu() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("POMODORO");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // Vertical snapping carousel: [Top, Center (Selected), Bottom]
  int midY = 32;
  int spacing = 14;

  int indices[3] = {
    (selectedPomoMode - 1 + 4) % 4,
    selectedPomoMode,
    (selectedPomoMode + 1) % 4
  };

  int yPositions[3] = {
    midY - spacing,
    midY,
    midY + spacing
  };

  for (int i = 0; i < 3; i++) {
    int modeIdx = indices[i];
    int drawY = yPositions[i];
    bool isSelected = (i == 1);

    if (isSelected) {
      display.fillRect(0, drawY - 4, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    int textW = strlen(pomoModes[modeIdx]) * 6;
    display.setCursor(64 - (textW/2), drawY - 2);
    display.print(pomoModes[modeIdx]);
  }
  display.setTextColor(SSD1306_WHITE);
}

void drawCustomTimeInput(bool editingMins) {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("POMODORO: CUSTOM");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setCursor(4, 18); // Centered text to prevent wrap-around clipping
  display.print("Set Session Duration");

  display.setTextSize(2);
  
  // Minutes Input Field
  if (editingMins) {
    display.fillRect(18, 32, 32, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(22, 34);
  display.printf("%02d", customMinutes);
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(54, 40);
  display.print("m");

  // Colon Separator
  display.setTextSize(2);
  display.setCursor(68, 34);
  display.print(":");

  // Seconds Input Field
  if (!editingMins) {
    display.fillRect(80, 32, 32, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(84, 34);
  display.printf("%02d", customSeconds);
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(116, 40);
  display.print("s");
}

void drawPomoCountdown(const char* header) {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("POMODORO: ");
  display.print(header);
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  int mins = pomoLeftSeconds / 60;
  int secs = pomoLeftSeconds % 60;
  char countStr[10];
  sprintf(countStr, "%02d:%02d", mins, secs);

  // Large timer count
  display.setTextSize(3);
  int w = strlen(countStr) * 18;
  display.setCursor(64 - (w/2), 18);
  display.print(countStr);

  // Inverted Progress Bar (starts empty, fills up to full as time runs down)
  int barY = 48;
  int barH = 8;
  display.drawRect(0, barY, 128, barH, SSD1306_WHITE);
  
  int filledWidth = 0;
  if (pomoTotalSeconds > 0) {
    float progress = (float)(pomoTotalSeconds - pomoLeftSeconds) / pomoTotalSeconds;
    filledWidth = (int)(progress * 124); // Account for border spacing
  }
  if (filledWidth > 0) {
    display.fillRect(2, barY + 2, filledWidth, barH - 4, SSD1306_WHITE);
  }

  // Back indicator
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 57);
  display.print("[Hold SW to Menu]");
}

void drawPomoFinished() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("POMODORO: DONE");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(16, 16);
  display.print("CONGRATS!");

  display.setTextSize(1);
  int restartX = 16;
  int exitX = 84;
  int optionY = 40;

  // Restart option
  if (pomoFinishedSelect == 0) {
    display.fillRect(restartX - 4, optionY - 2, 50, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(restartX, optionY);
  display.print("Restart");

  // Exit option
  if (pomoFinishedSelect == 1) {
    display.fillRect(exitX - 4, optionY - 2, 32, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(exitX, optionY);
  display.print("Exit");

  display.setTextColor(SSD1306_WHITE);
}

void drawPomodoroApp() {
  switch (currentPomoState) {
    case POMO_STATE_MENU:        drawPomoMenu(); break;
    case POMO_STATE_CUSTOM_MINS: drawCustomTimeInput(true); break;
    case POMO_STATE_CUSTOM_SECS: drawCustomTimeInput(false); break;
    case POMO_STATE_RUNNING:     drawPomoCountdown("RUNNING"); break;
    case POMO_STATE_PAUSED:      drawPomoCountdown("PAUSED"); break;
    case POMO_STATE_FINISHED:    drawPomoFinished(); break;
  }
}

/* ========================================================================== */
/*                              ALARM APPLICATION                             */
/* ========================================================================== */



void drawAlarmApp() {
  if (alarmActive) {
    display.setTextSize(1);
    display.setCursor(4, 2);
    display.print("ALARM ACTIVE");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    display.setTextSize(3);
    display.setCursor(8, 22);
    display.print("ALARM!!!");

    display.setTextSize(1);
    display.setCursor(8, 50);
    display.print("[Click SW to Stop]");
    return;
  }

  // Header
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("ALARM CLOCK");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  if (currentAlarmState == ALARM_STATE_MENU) {
    // Render alarm status metrics
    display.setCursor(10, 18);
    display.print("Status: ");
    display.print(alarmEnabled ? "ON" : "OFF");

    display.setCursor(10, 28);
    display.print("Time  : ");
    if (use12HourFormat) {
      int hr = alarmHour;
      const char* ampm = (hr >= 12) ? "PM" : "AM";
      hr = hr % 12;
      if (hr == 0) hr = 12;
      display.printf("%d:%02d %s", hr, alarmMinute, ampm);
    } else {
      display.printf("%02d:%02d", alarmHour, alarmMinute);
    }

    int toggleX = 16;
    int setX = 80;
    int optionY = 44;

    // Toggle menu button
    if (alarmMenuSelect == 0) {
      display.fillRect(toggleX - 4, optionY - 2, 44, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(toggleX, optionY);
    display.print("Toggle");

    // Set Time menu button
    if (alarmMenuSelect == 1) {
      display.fillRect(setX - 4, optionY - 2, 34, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(setX, optionY);
    display.print("Set");

    display.setTextColor(SSD1306_WHITE);
  }
  else if (currentAlarmState == ALARM_STATE_SET_HOUR || currentAlarmState == ALARM_STATE_SET_MINUTE) {
    display.setCursor(14, 18);
    display.print("Adjust Alarm Time");

    display.setTextSize(2);
    
    // Hour field input box
    if (currentAlarmState == ALARM_STATE_SET_HOUR) {
      display.fillRect(24, 32, 32, 20, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(28, 34);
    display.printf("%02d", tempAlarmHour);

    // Separator colon
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(62, 34);
    display.print(":");

    // Minute field input box
    if (currentAlarmState == ALARM_STATE_SET_MINUTE) {
      display.fillRect(72, 32, 32, 20, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(76, 34);
    display.printf("%02d", tempAlarmMinute);

    display.setTextColor(SSD1306_WHITE);
  }
}

/* ========================================================================== */
/*                          INPUT HANDLERS & SYSTEM                           */
/* ========================================================================== */

int readEncoderDelta() {
  int currentClkState = digitalRead(ENCODER_CLK);
  int delta = 0;
  if (currentClkState != lastClkState) {
    lastClkState = currentClkState;
    if (currentClkState == LOW) {
      if (digitalRead(ENCODER_DT) == HIGH) delta = -1;
      else delta = 1;
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
  static bool triggeredHomePress = false;
  
  int reading = digitalRead(ENCODER_SW);
  ButtonEvent event = BUTTON_NONE;
  unsigned long now = millis();
  
  if (reading == HIGH) buttonReleasedSinceBoot = true;
  
  if (reading == LOW && lastButtonState == HIGH) {
    buttonDownTime = now;
    isHolding = true;
    triggeredLongPress = false;
    triggeredHomePress = false;
  } 
  else if (reading == HIGH && lastButtonState == LOW) {
    if (isHolding) {
      unsigned long duration = now - buttonDownTime;
      if (duration >= 3000) {
        if (buttonReleasedSinceBoot) event = BUTTON_SLEEP;
      } else if (duration >= 1500) {
        if (buttonReleasedSinceBoot && !triggeredHomePress) event = BUTTON_HOME_PRESS;
      } else if (duration >= 600) {
        if (buttonReleasedSinceBoot && !triggeredLongPress) event = BUTTON_LONG_PRESS;
      } else if (duration >= 5) {
        event = BUTTON_CLICK;
      }
      isHolding = false;
    }
  } 
  else if (reading == LOW && isHolding) {
    unsigned long duration = now - buttonDownTime;
    if (duration >= 3000) {
      if (buttonReleasedSinceBoot) {
        event = BUTTON_SLEEP;
        isHolding = false; 
      }
    } else if (duration >= 1500 && !triggeredHomePress) {
      if (buttonReleasedSinceBoot) {
        event = BUTTON_HOME_PRESS;
        triggeredHomePress = true; 
      }
    } else if (duration >= 600 && !triggeredLongPress) {
      if (buttonReleasedSinceBoot) {
        event = BUTTON_LONG_PRESS;
        triggeredLongPress = true; 
      }
    }
  }
  
  lastButtonState = reading;
  return event;
}

void updateBatteryMeasurements() {
  int raw = analogRead(BATTERY_PIN);
  globalBatVolts = (raw / 4095.0) * 3.1 * 2.0;
  globalBatVolts = constrain(globalBatVolts, 3.2, 4.2);
  globalBatteryPct = (int)((globalBatVolts - 3.2) / 1.0 * 100.0);
  globalBatteryPct = constrain(globalBatteryPct, 0, 100);
}

void goToSleep() {
  tone(BUZZER_PIN, 1800, 100); delay(120);
  tone(BUZZER_PIN, 1200, 100); delay(120);
  tone(BUZZER_PIN, 800, 150); delay(200);
  noTone(BUZZER_PIN);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  digitalWrite(LED_PIN, LOW);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << ENCODER_SW, ESP_GPIO_WAKEUP_GPIO_LOW);

  if (alarmEnabled) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    
    int currentMins = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int alarmMins = alarmHour * 60 + alarmMinute;
    int diffMins = alarmMins - currentMins;
    if (diffMins <= 0) diffMins += 1440; // Next day
    
    // Calculate exact seconds so we wake up right on time
    int diffSeconds = (diffMins * 60) - timeinfo->tm_sec;
    
    esp_sleep_enable_timer_wakeup((uint64_t)diffSeconds * 1000000ULL);
  }

  esp_deep_sleep_start();
}

void playTick() {
  if (systemVolume == 0) return;
  int freq = (systemVolume == 1) ? 1000 : (systemVolume == 2) ? 2000 : 2400;
  tone(BUZZER_PIN, freq, 8);
}

void playSelectTone() {
  if (systemVolume == 0) return;
  tone(BUZZER_PIN, (systemVolume == 1) ? 1200 : 1800, 50);
  delay(60);
  tone(BUZZER_PIN, (systemVolume == 1) ? 1600 : 2200, 80);
}

void playCancelTone() {
  if (systemVolume == 0) return;
  tone(BUZZER_PIN, (systemVolume == 1) ? 800 : 1200, 100);
  delay(120);
  tone(BUZZER_PIN, (systemVolume == 1) ? 500 : 800, 150);
}

void playFinishedMelody() {
  if (systemVolume == 0) return;
  int melody[] = { 262, 330, 392, 523, 392, 523 };
  int durations[] = { 150, 150, 150, 200, 150, 300 };
  for (int i = 0; i < 6; i++) {
    tone(BUZZER_PIN, (systemVolume == 1) ? melody[i]/2 : melody[i], durations[i]);
    delay(durations[i] + 20);
  }
  noTone(BUZZER_PIN);
}

void setDisplayBrightness(int level) {
  int contrast = 5;
  if (level == 1) contrast = 80;
  else if (level == 2) contrast = 255;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}



void drawSettingsMenu() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SETTINGS");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  const char* options[5] = {"Set Time", "Set Date", "Time Format", "Volume", "Brightness"};
  int midY = 32;
  int spacing = 12;

  int indices[3] = {
    (settingsMenuSelect - 1 + 5) % 5,
    settingsMenuSelect,
    (settingsMenuSelect + 1) % 5
  };

  int yPositions[3] = {
    midY - spacing,
    midY,
    midY + spacing
  };

  for (int i = 0; i < 3; i++) {
    int optIdx = indices[i];
    int drawY = yPositions[i];
    bool isSelected = (i == 1);

    if (isSelected) {
      display.fillRect(0, drawY - 3, 128, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(10, drawY - 2);
    display.print(options[optIdx]);
    
    // Render status indicator next to settings slots
    if (optIdx == 2) {
      display.setCursor(90, drawY - 2);
      display.print(use12HourFormat ? "12-HR" : "24-HR");
    } else if (optIdx == 3) {
      // Mini Volume Progress Bar
      int miniBarX = 90;
      int miniBarY = drawY - 2;
      int miniBarW = 30;
      int miniBarH = 7;
      display.drawRect(miniBarX, miniBarY, miniBarW, miniBarH, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
      int fillVal = (systemVolume * (miniBarW - 4)) / 3;
      if (fillVal > 0) {
        display.fillRect(miniBarX + 2, miniBarY + 2, fillVal, miniBarH - 4, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
      }
    } else if (optIdx == 4) {
      // Mini Brightness Progress Bar
      int miniBarX = 90;
      int miniBarY = drawY - 2;
      int miniBarW = 30;
      int miniBarH = 7;
      display.drawRect(miniBarX, miniBarY, miniBarW, miniBarH, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
      int fillVal = ((systemBrightness + 1) * (miniBarW - 4)) / 3;
      if (fillVal > 0) {
        display.fillRect(miniBarX + 2, miniBarY + 2, fillVal, miniBarH - 4, isSelected ? SSD1306_BLACK : SSD1306_WHITE);
      }
    }
  }
  display.setTextColor(SSD1306_WHITE);
}

void drawSettingsTimeSet() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SETTINGS: SET TIME");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setCursor(14, 18);
  display.print("Adjust System Time");

  display.setTextSize(2);
  
  // Hour Input Field
  if (currentSettingsState == SET_STATE_TIME_HH) {
    display.fillRect(24, 32, 32, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(28, 34);
  display.printf("%02d", tempHour);

  // Colon Separator
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(62, 34);
  display.print(":");

  // Minute Input Field
  if (currentSettingsState == SET_STATE_TIME_MM) {
    display.fillRect(72, 32, 32, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(76, 34);
  display.printf("%02d", tempMinute);

  display.setTextColor(SSD1306_WHITE);
}

void drawSettingsDateSet() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SETTINGS: SET DATE");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setCursor(14, 18);
  display.print("Adjust System Date");

  display.setTextSize(2);
  
  // Day Field
  if (currentSettingsState == SET_STATE_DATE_DD) {
    display.fillRect(10, 32, 28, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(12, 34);
  display.printf("%02d", tempDay);

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 34);
  display.print("/");

  // Month Field
  if (currentSettingsState == SET_STATE_DATE_MM) {
    display.fillRect(48, 32, 28, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(50, 34);
  display.printf("%02d", tempMonth);

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(78, 34);
  display.print("/");

  // Year Field (last 2 digits)
  if (currentSettingsState == SET_STATE_DATE_YY) {
    display.fillRect(86, 32, 32, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(88, 34);
  display.printf("%02d", tempYear % 100);

  display.setTextColor(SSD1306_WHITE);
}

void drawSettingsVolume() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SETTINGS: VOLUME");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setCursor(16, 20);
  display.print("Adjust System Volume");

  // Progress Bar Frame
  int barX = 24;
  int barY = 34;
  int barW = 80;
  int barH = 12;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);

  // Filled width based on volume level
  int filledW = 0;
  if (systemVolume == 1) filledW = barW / 3;
  else if (systemVolume == 2) filledW = (barW * 2) / 3;
  else if (systemVolume == 3) filledW = barW;

  if (filledW > 0) {
    display.fillRect(barX + 2, barY + 2, filledW - 4, barH - 4, SSD1306_WHITE);
  }

  // Text Label below
  const char* volStrings[4] = {"MUTE", "LOW", "MED", "HIGH"};
  display.setCursor(64 - (strlen(volStrings[systemVolume]) * 3), 50);
  display.print(volStrings[systemVolume]);
}

void drawSettingsBrightness() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SETTINGS: CONTRAST");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setCursor(10, 20);
  display.print("Adjust Screen Contrast");

  // Progress Bar Frame
  int barX = 24;
  int barY = 34;
  int barW = 80;
  int barH = 12;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);

  // Filled width based on brightness level
  int filledW = 0;
  if (systemBrightness == 0) filledW = barW / 3;
  else if (systemBrightness == 1) filledW = (barW * 2) / 3;
  else if (systemBrightness == 2) filledW = barW;

  if (filledW > 0) {
    display.fillRect(barX + 2, barY + 2, filledW - 4, barH - 4, SSD1306_WHITE);
  }

  // Text Label below
  const char* brightStrings[3] = {"LOW", "MED", "HIGH"};
  display.setCursor(64 - (strlen(brightStrings[systemBrightness]) * 3), 50);
  display.print(brightStrings[systemBrightness]);
}



void updateLED() {
  unsigned long now = millis();
  
  if (alarmActive) {
    // Alarm ringing: aggressive flashing indicator (10Hz)
    if ((now / 50) % 2 == 0) analogWrite(LED_PIN, 255);
    else analogWrite(LED_PIN, 0);
  }
  else if (currentOSState == OS_APP_POMODORO && currentPomoState == POMO_STATE_RUNNING) {
    // Breathing effect (sine wave)
    float intensity = 127.0 + 127.0 * sin(now / 500.0 * PI);
    analogWrite(LED_PIN, (int)intensity);
  } else if (currentOSState == OS_APP_POMODORO && (currentPomoState == POMO_STATE_PAUSED || currentPomoState == POMO_STATE_FINISHED)) {
    // Flashing effect (2Hz)
    if ((now / 250) % 2 == 0) {
      analogWrite(LED_PIN, 255);
    } else {
      analogWrite(LED_PIN, 0);
    }
  } else {
    // Solid ON for general menus and launcher state
    analogWrite(LED_PIN, 255);
  }
}

/* ========================================================================== */
/*                                TETRIS APP                                  */
/* ========================================================================== */









void drawTetrisApp() {
  if (currentTetrisState == TETRIS_MENU) {
    display.setTextSize(1);
    display.setCursor(14, 10);
    display.print("TETRIS");
    display.drawFastHLine(0, 22, 64, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(8, 40);
    display.print("PLAY");

    display.setTextSize(1);
    display.setCursor(8, 80);
    display.print("[Click]");
  }
  else if (currentTetrisState == TETRIS_GAMEOVER) {
    display.setTextSize(2);
    display.setCursor(8, 20);
    display.print("GAME");
    display.setCursor(8, 40);
    display.print("OVER");

    display.setTextSize(1);
    display.setCursor(8, 70);
    display.print("Scr:");
    display.print(tetrisScore);

    display.setCursor(8, 100);
    display.print("[Click]");
  }
  else if (currentTetrisState == TETRIS_PLAYING) {
    int boardPX = TETRIS_X_OFFSET - 1;
    int boardPY = 16;
    int boardPW = (TETRIS_COLS * BLOCK_SIZE) + 2;
    int boardPH = (TETRIS_ROWS * BLOCK_SIZE) + 2;
    display.drawRect(boardPX, boardPY, boardPW, boardPH, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(2, 4);
    display.print("SCORE: ");
    display.print(tetrisScore);

    for (int r = 0; r < TETRIS_ROWS; r++) {
      for (int c = 0; c < TETRIS_COLS; c++) {
        if (tetrisBoard[r] & (1 << c)) {
          display.fillRect(TETRIS_X_OFFSET + (c * BLOCK_SIZE), boardPY + 1 + (r * BLOCK_SIZE), BLOCK_SIZE - 1, BLOCK_SIZE - 1, SSD1306_WHITE);
        }
      }
    }

    uint16_t shape = tetrominoes[currentPieceType][currentPieceRot];
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 4; c++) {
        if (shape & (1 << (15 - (r * 4 + c)))) {
          int boardY = currentPieceY + r;
          int boardX = currentPieceX + c;
          if (boardY >= 0 && boardY < TETRIS_ROWS) {
            display.fillRect(TETRIS_X_OFFSET + (boardX * BLOCK_SIZE), boardPY + 1 + (boardY * BLOCK_SIZE), BLOCK_SIZE - 1, BLOCK_SIZE - 1, SSD1306_WHITE);
          }
        }
      }
    }
  }
}

void loop() {
  unsigned long now = millis();
  bool uiChanged = false;

  // 0. Global Alarm Checker
  static int lastRungMinute = -1;
  struct timeval tv_alarm;
  gettimeofday(&tv_alarm, NULL);
  struct tm* timeinfo_alarm = localtime(&tv_alarm.tv_sec);
  
  if (alarmEnabled && !alarmActive) {
    if (timeinfo_alarm->tm_hour == alarmHour && timeinfo_alarm->tm_min == alarmMinute && timeinfo_alarm->tm_min != lastRungMinute) {
      alarmActive = true;
      lastRungMinute = timeinfo_alarm->tm_min;
      currentOSState = OS_APP_ALARM;
      currentAlarmState = ALARM_STATE_RINGING;
      uiChanged = true;
    }
  }

  // Active Alarm ringing sound (2Hz beep)
  if (alarmActive) {
    static unsigned long lastAlarmBeep = 0;
    if (now - lastAlarmBeep >= 500) {
      lastAlarmBeep = now;
      if (systemVolume > 0) {
        tone(BUZZER_PIN, (systemVolume == 1) ? 1000 : 2000, 200);
      }
      uiChanged = true; // Keep screen flashing/redrawing
    }
  }

  // 1. App Carousel Position (Instant Snap - no-op)

  // 2. RTC Clock Tick (Update launcher every second)
  static unsigned long lastClockUpdate = 0;
  if (currentOSState == OS_LAUNCHER && (now - lastClockUpdate >= 1000)) {
    lastClockUpdate = now;
    uiChanged = true;
  }

  // 3. Battery Tick (non-blocking)
  if (now - lastBatteryCheckTime >= 2000) {
    lastBatteryCheckTime = now;
    updateBatteryMeasurements();
    uiChanged = true;
  }

  // 3.5. Pomodoro Timer Countdown Tick (1Hz)
  if (currentOSState == OS_APP_POMODORO && currentPomoState == POMO_STATE_RUNNING) {
    static unsigned long lastPomoTick = 0;
    if (now - lastPomoTick >= 1000) {
      lastPomoTick = now;
      if (pomoLeftSeconds > 0) {
        pomoLeftSeconds--;
        uiChanged = true;
      }
      if (pomoLeftSeconds <= 0) {
        currentPomoState = POMO_STATE_FINISHED;
        uiChanged = true;
        playFinishedMelody();
      }
    }
  }

  // 3.6 Tetris Gravity Tick
  if (currentOSState == OS_APP_TETRIS && currentTetrisState == TETRIS_PLAYING) {
    if (now - lastTetrisDrop >= tetrisSpeed) {
      lastTetrisDrop = now;
      if (!checkTetrisCollision(currentPieceType, currentPieceRot, currentPieceX, currentPieceY + 1)) {
        currentPieceY++;
      } else {
        lockTetrisPiece();
      }
      uiChanged = true;
    }
  }

  // 3.7 Reader Tick
  if (currentOSState == OS_APP_READER && currentReaderState == READER_PLAYING) {
    unsigned long wordDelay = 60000 / readerWpm;
    if (now - lastReaderWordTime >= wordDelay) {
      lastReaderWordTime = now;
      readerWordIndex++;
      if (readerWordIndex >= readerTotalWords) {
        currentReaderState = READER_FINISHED;
      }
      uiChanged = true;
    }
  }

  // 4. Read Inputs
  int encoderDelta = readEncoderDelta();
  ButtonEvent btnEvent = checkButton();
  if (encoderDelta != 0 || btnEvent != BUTTON_NONE) {
    uiChanged = true;
    lastActivityTime = now;
  }

  // 4.5 Idle Sleep Timer
  if (now - lastActivityTime >= 60000) {
    bool canSleep = true;
    if (currentOSState == OS_APP_POMODORO && currentPomoState == POMO_STATE_RUNNING) canSleep = false;
    if (currentOSState == OS_APP_READER && currentReaderState == READER_PLAYING) canSleep = false;
    if (alarmActive) canSleep = false;
    if (canSleep) goToSleep();
  }

  // 5. Global Actions
  if (btnEvent == BUTTON_SLEEP) {
    goToSleep();
  } else if (btnEvent == BUTTON_HOME_PRESS) {
    if (currentOSState == OS_APP_TETRIS) display.setRotation(0);
    currentOSState = OS_LAUNCHER;
    playCancelTone();
  }

  // 6. TimOS State Machine
  switch (currentOSState) {
    
    // --- HOME SCREEN ---
    case OS_LAUNCHER:
      if (btnEvent == BUTTON_CLICK) {
        currentOSState = OS_APP_MENU;
        playSelectTone();
      }
      break;

    // --- APP CAROUSEL ---
    case OS_APP_MENU:
      if (encoderDelta != 0) {
        selectedAppIndex += encoderDelta;
        if (selectedAppIndex < 0) selectedAppIndex = NUM_APPS - 1;
        if (selectedAppIndex >= NUM_APPS) selectedAppIndex = 0;
      }
      if (btnEvent == BUTTON_CLICK) {
        // Open Selected App
        if (selectedAppIndex == 0) currentOSState = OS_APP_POMODORO;
        else if (selectedAppIndex == 1) currentOSState = OS_APP_ALARM;
        else if (selectedAppIndex == 2) currentOSState = OS_APP_SETTINGS;
        else if (selectedAppIndex == 3) {
          currentOSState = OS_APP_TETRIS;
          display.setRotation(3); // Inverted Portrait mode
        }
        else if (selectedAppIndex == 4) currentOSState = OS_APP_READER;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentOSState = OS_LAUNCHER; // Back to home
        playCancelTone();
      }
      break;

    // --- APPS (TimOS Applications) ---
    case OS_APP_POMODORO:
      if (currentPomoState == POMO_STATE_MENU) {
        if (encoderDelta != 0) {
          selectedPomoMode += encoderDelta;
          if (selectedPomoMode < 0) selectedPomoMode = 3;
          if (selectedPomoMode > 3) selectedPomoMode = 0;
        }
        if (btnEvent == BUTTON_CLICK) {
          if (selectedPomoMode < 3) {
            // Preset Times
            if (selectedPomoMode == 0) pomoTotalSeconds = 25 * 60;
            else if (selectedPomoMode == 1) pomoTotalSeconds = 5 * 60;
            else if (selectedPomoMode == 2) pomoTotalSeconds = 15 * 60;
            pomoLeftSeconds = pomoTotalSeconds;
            currentPomoState = POMO_STATE_RUNNING;
            playSelectTone();
          } else {
            // Custom Time Configuration
            customMinutes = 25;
            customSeconds = 0;
            currentPomoState = POMO_STATE_CUSTOM_MINS;
            playSelectTone();
          }
        }
        if (btnEvent == BUTTON_LONG_PRESS) {
          currentOSState = OS_APP_MENU; // Back to app launcher
          playCancelTone();
        }
      }
      else if (currentPomoState == POMO_STATE_CUSTOM_MINS) {
        if (encoderDelta != 0) {
          customMinutes += encoderDelta;
          if (customMinutes < 1) customMinutes = 99; // Constrain between 1 and 99 mins
          if (customMinutes > 99) customMinutes = 1;
        }
        if (btnEvent == BUTTON_CLICK) {
          currentPomoState = POMO_STATE_CUSTOM_SECS;
          playSelectTone();
        }
        if (btnEvent == BUTTON_LONG_PRESS) {
          currentPomoState = POMO_STATE_MENU;
          playCancelTone();
        }
      }
      else if (currentPomoState == POMO_STATE_CUSTOM_SECS) {
        if (encoderDelta != 0) {
          customSeconds += encoderDelta;
          if (customSeconds < 0) customSeconds = 59;
          if (customSeconds > 59) customSeconds = 0;
        }
        if (btnEvent == BUTTON_CLICK) {
          pomoTotalSeconds = (customMinutes * 60) + customSeconds;
          if (pomoTotalSeconds > 0) {
            pomoLeftSeconds = pomoTotalSeconds;
            currentPomoState = POMO_STATE_RUNNING;
            playSelectTone();
          } else {
            playCancelTone();
          }
        }
        if (btnEvent == BUTTON_LONG_PRESS) {
          currentPomoState = POMO_STATE_CUSTOM_MINS;
          playCancelTone();
        }
      }
      else if (currentPomoState == POMO_STATE_RUNNING) {
        if (btnEvent == BUTTON_CLICK) {
          currentPomoState = POMO_STATE_PAUSED;
          playSelectTone();
        }
        if (btnEvent == BUTTON_LONG_PRESS) {
          currentPomoState = POMO_STATE_MENU;
          playCancelTone();
        }
      }
      else if (currentPomoState == POMO_STATE_PAUSED) {
        if (btnEvent == BUTTON_CLICK) {
          currentPomoState = POMO_STATE_RUNNING;
          playSelectTone();
        }
        if (btnEvent == BUTTON_LONG_PRESS) {
          currentPomoState = POMO_STATE_MENU;
          playCancelTone();
        }
      }
      else if (currentPomoState == POMO_STATE_FINISHED) {
        if (encoderDelta != 0) {
          pomoFinishedSelect = (pomoFinishedSelect == 0) ? 1 : 0;
        }
        if (btnEvent == BUTTON_CLICK) {
          if (pomoFinishedSelect == 0) { // Restart
            pomoLeftSeconds = pomoTotalSeconds;
            currentPomoState = POMO_STATE_RUNNING;
            playSelectTone();
          } else { // Exit
            currentPomoState = POMO_STATE_MENU;
            playCancelTone();
          }
        }
        if (btnEvent == BUTTON_LONG_PRESS) {
          currentOSState = OS_APP_MENU;
          playCancelTone();
        }
      }
      break;

    case OS_APP_ALARM:
      handleAlarmApp(encoderDelta, btnEvent);
      break;

    case OS_APP_SETTINGS:
      handleSettingsApp(encoderDelta, btnEvent);
      break;

    case OS_APP_TETRIS:
      handleTetrisApp(encoderDelta, btnEvent);
      break;

    case OS_APP_READER:
      handleReaderApp(encoderDelta, btnEvent);
      break;
  }

  // 7. Draw UI Layer
  if (uiChanged) {
    display.clearDisplay();
    drawBatteryStatus();
    
    if (currentOSState == OS_LAUNCHER) {
      drawLauncher();
    } else if (currentOSState == OS_APP_MENU) {
      drawAppMenu();
    } else if (currentOSState == OS_APP_POMODORO) {
      drawPomodoroApp();
    } else if (currentOSState == OS_APP_ALARM) {
      drawAlarmApp();
    } else if (currentOSState == OS_APP_SETTINGS) {
      drawSettingsApp();
    } else if (currentOSState == OS_APP_TETRIS) {
      drawTetrisApp();
    } else if (currentOSState == OS_APP_READER) {
      drawReaderApp();
    }
    
    display.display();
  }
  
  // 8. Update LED status (breathing, flashing, solid)
  updateLED();

  delay(1);
}
