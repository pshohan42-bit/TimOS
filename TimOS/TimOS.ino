/**
 * TimOS: ESP32C3-Pro Smart Timer Operating System
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_sleep.h"
#include <sys/time.h>
#include <Preferences.h>
#include "CustomReaderFont.h"

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
int menuScrollAnimY = 0;

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
time_t pomoStartEpoch = 0;
int pomoStartLeft = 0;

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
enum ReaderState { READER_HOME, READER_BOOK_SELECT, READER_CHAPTER_SELECT, READER_MENU, READER_PLAYING, READER_PAUSED, READER_FINISHED, READER_SETTINGS };
int readerPuncMode = 0; // 0=Norm, 1=Long, 2=Off
int readerLongMode = 0; // 0=Norm, 1=Long, 2=Off
int readerFontMode = 0; // 0=Auto, 1=Big, 2=Med, 3=Small
int readerSetMenuIdx = 0;
ReaderState currentReaderState = READER_HOME;
unsigned long readerMarqueeStartTime = 0;
int readerHomeOption = 0;
int readerWpm = 250;
int readerWordIndex = 0;
int readerTotalWords = 0;
unsigned long lastReaderWordTime = 0;
int sessionWordsRead = 0;

// Battery
float globalBatVolts = 0.0;
int globalBatteryPct = 100;
int prevBatteryPct = -1;
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
String getCurrentWord(int targetIdx);
void handleAlarmApp(int encoderDelta, ButtonEvent btnEvent);
void handleSettingsApp(int encoderDelta, ButtonEvent btnEvent);
void handleTetrisApp(int encoderDelta, ButtonEvent btnEvent);
void handleReaderApp(int encoderDelta, ButtonEvent btnEvent);
void handlePomodoroApp(int encoderDelta, ButtonEvent btnEvent);
bool checkTetrisCollision(int pType, int pRot, int pX, int pY);
void lockTetrisPiece();
void spawnTetrisPiece();

void setup() {
  Serial.begin(115200);

  preferences.begin("timos", false);
  systemVolume = preferences.getInt("vol", 2);
  systemBrightness = preferences.getInt("bright", 2);
  use12HourFormat = preferences.getBool("12hr", false);
  alarmEnabled = preferences.getBool("alarmEn", false);
  alarmHour = preferences.getInt("alarmHr", 6);
  alarmMinute = preferences.getInt("alarmMin", 0);
  readerWpm = preferences.getInt("wpm", 250);
  readerPuncMode = preferences.getInt("rPunc", 0);
  readerLongMode = preferences.getInt("rLong", 0);
  readerFontMode = preferences.getInt("rFont", 0);
  readerPuncMode = preferences.getInt("rPunc", 0);
  readerLongMode = preferences.getInt("rLong", 0);
  readerFontMode = preferences.getInt("rFont", 0); // Restore WPM (#22)

  // Initialize RTC time (dummy time for testing Launcher)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (tv.tv_sec < 100000) {
    tv.tv_sec = 1718000000; // Arbitrary timestamp
    settimeofday(&tv, NULL);
  }

  // Configure Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, HIGH);
  lastClkState = digitalRead(ENCODER_CLK);

  // Initialize OLED
  Wire.begin(5, 6); // SDA = GPIO5, SCL = GPIO6
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while(true) delay(100);
  }
  
  display.ssd1306_command(0xA0); // Un-mirror
  setDisplayBrightness(systemBrightness);
  display.setTextColor(SSD1306_WHITE);
  
  updateBatteryMeasurements();
  lastActivityTime = millis();

  // Show Boot Splash Screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(34, 24);
  display.print("TimOS");
  display.display();
  delay(1000);
}


void drawLauncher() {
  drawBatteryStatus(); // Launcher needs its own battery call since it skips unified top bar
  
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm* timeinfo = localtime(&tv.tv_sec);
  
  char digitsStr[10];
  const char* ampm = "";
  
  if (use12HourFormat) {
    int hr = timeinfo->tm_hour;
    ampm = (hr >= 12) ? "PM" : "AM";
    hr = hr % 12;
    if (hr == 0) hr = 12;
    sprintf(digitsStr, "%d:%02d", hr, timeinfo->tm_min);
  } else {
    strftime(digitsStr, sizeof(digitsStr), "%H:%M", timeinfo);
  }
  
  // Format Date (e.g. "Mon, 15 Jul")
  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %d %b", timeinfo);

  // Big Clock Digits
  display.setTextSize(3);
  int digitsWidth = strlen(digitsStr) * 18; 
  int startX = 64 - (digitsWidth / 2);
  
  // If showing AM/PM, offset slightly left to balance the AM/PM width
  if (use12HourFormat) {
    startX = 64 - ((digitsWidth + 14) / 2);
  }
  
  display.setCursor(startX, 16);
  display.print(digitsStr);
  
  // Small AM/PM
  if (use12HourFormat) {
    display.setTextSize(1);
    display.setCursor(startX + digitsWidth + 2, 16);
    display.print(ampm);
  }
  
  // Small Date
  display.setTextSize(1);
  int dateWidth = strlen(dateStr) * 6;
  display.setCursor(64 - (dateWidth/2), 48);
  display.print(dateStr);
}

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
  
  // Circular App Indices: [FarLeft, Left, Center, Right, FarRight]
  int appIndices[5] = {
    (selectedAppIndex - 2 + NUM_APPS * 2) % NUM_APPS,
    (selectedAppIndex - 1 + NUM_APPS * 2) % NUM_APPS,
    selectedAppIndex,
    (selectedAppIndex + 1) % NUM_APPS,
    (selectedAppIndex + 2) % NUM_APPS
  };

  int xPositions[5] = {
    centerX - (iconSpacing * 2) + menuScrollAnimY,
    centerX - iconSpacing + menuScrollAnimY,
    centerX + menuScrollAnimY,
    centerX + iconSpacing + menuScrollAnimY,
    centerX + (iconSpacing * 2) + menuScrollAnimY
  };

  // 1. Draw FIXED selection box at center
  display.fillRoundRect(centerX - (boxSize/2), boxY, boxSize, boxSize, 4, SSD1306_WHITE);
  
  // 2. Draw fixed text below the selection box (so the text doesn't slide)
  display.setTextColor(SSD1306_WHITE);
  int textW = strlen(appNames[selectedAppIndex]) * 6;
  display.setCursor(centerX - (textW/2), boxY + boxSize + 6);
  display.print(appNames[selectedAppIndex]);

  // 3. Draw sliding icons in INVERSE mode
  display.setTextColor(SSD1306_INVERSE);

  for (int slot = 0; slot < 5; slot++) {
    int appIdx = appIndices[slot];
    int iconX = xPositions[slot];
    
    // Icon background boundary (unselected items get an outline)
    display.drawRoundRect(iconX - (boxSize/2), boxY, boxSize, boxSize, 4, SSD1306_INVERSE);
    
    // Draw custom vector icon shapes INVERSE
    if (appIdx == 0) { // Pomodoro: Circle timer
       display.drawCircle(iconX, boxY + 14, 7, SSD1306_INVERSE);
       display.drawLine(iconX, boxY + 14, iconX, boxY + 8, SSD1306_INVERSE);
    } else if (appIdx == 1) { // Alarm: Bell triangle
       display.fillTriangle(iconX, boxY + 6, iconX - 7, boxY + 18, iconX + 7, boxY + 18, SSD1306_INVERSE);
    } else if (appIdx == 2) { // Settings: Gear/Sliders
       display.drawRect(iconX - 5, boxY + 9, 10, 10, SSD1306_INVERSE);
       display.drawRect(iconX - 2, boxY + 5, 4, 18, SSD1306_INVERSE);
       display.drawRect(iconX - 8, boxY + 12, 16, 4, SSD1306_INVERSE);
    } else if (appIdx == 3) { // Tetris: T-Block shape
       display.fillRect(iconX - 6, boxY + 6, 12, 4, SSD1306_INVERSE);
       display.fillRect(iconX - 2, boxY + 10, 4, 4, SSD1306_INVERSE);
    } else if (appIdx == 4) { // Reader: Book shape
       display.drawRect(iconX - 7, boxY + 8, 7, 10, SSD1306_INVERSE);
       display.drawRect(iconX, boxY + 8, 7, 10, SSD1306_INVERSE);
       display.drawLine(iconX, boxY + 8, iconX, boxY + 18, SSD1306_INVERSE);
    }
  }
  display.setTextColor(SSD1306_WHITE);
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

void drawTopBar() {
  if (currentOSState == OS_APP_TETRIS || currentOSState == OS_LAUNCHER) return;
  
  // Clear the top area so it overlays over any app's old top bar
  display.fillRect(0, 0, 128, 13, SSD1306_BLACK);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  
  if (currentOSState == OS_APP_MENU) display.print("APPS");
  else if (currentOSState == OS_APP_POMODORO) {
    if (currentPomoState == POMO_STATE_CUSTOM_MINS || currentPomoState == POMO_STATE_CUSTOM_SECS) display.print("CUSTOM TIME");
    else if (currentPomoState == POMO_STATE_FINISHED) display.print("POMODORO: DONE");
    else display.print("POMODORO");
  }
  else if (currentOSState == OS_APP_ALARM) {
    extern bool alarmActive;
    if (alarmActive) display.print("ALARM ACTIVE");
    else display.print("ALARM CLOCK");
  }
  else if (currentOSState == OS_APP_SETTINGS) display.print("SETTINGS");
  else if (currentOSState == OS_APP_READER) {
    extern String getReaderTopBarText();
    String txt = getReaderTopBarText();
    // Prevent overlap with time/battery by truncating if necessary
    if (txt.length() > 12) {
      txt = txt.substring(0, 10) + "..";
    }
    display.print(txt);
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm* timeinfo = localtime(&tv.tv_sec);
  char timeStr[10];
  if (use12HourFormat) {
    int hr = timeinfo->tm_hour % 12;
    if (hr == 0) hr = 12;
    sprintf(timeStr, "%d:%02d", hr, timeinfo->tm_min);
  } else {
    sprintf(timeStr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  }
  int timeW = strlen(timeStr) * 6;
  display.setCursor(104 - timeW, 2);
  display.print(timeStr);

  drawBatteryStatus();
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
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
    const char* options[2] = {"Toggle: ", "Set Time"};
    int midY = 32;
    int spacing = 14;
    extern int menuScrollAnimY;

    // 1. Draw FIXED selection box
    display.fillRect(0, midY - 3, 128, 13, SSD1306_WHITE);

    // 2. Draw sliding text in INVERSE mode
    display.setTextColor(SSD1306_INVERSE);

    for (int offset = -1; offset <= 1; offset++) {
      int optIdx = (alarmMenuSelect + offset + 2) % 2;
      
      int drawY = midY + (offset * spacing) + menuScrollAnimY;
      
      display.setCursor(10, drawY - 2);
      display.print(options[optIdx]);
      
      if (optIdx == 0) {
        display.print(alarmEnabled ? "ON" : "OFF");
      } else if (optIdx == 1) {
        display.setCursor(76, drawY - 2);
        if (use12HourFormat) {
          int hr = alarmHour;
          const char* ampm = (hr >= 12) ? "PM" : "AM";
          hr = hr % 12;
          if (hr == 0) hr = 12;
          display.printf("%d:%02d%s", hr, alarmMinute, ampm);
        } else {
          display.printf("%02d:%02d", alarmHour, alarmMinute);
        }
      }
    }

    // 3. Draw clipping mask and title
    display.fillRect(0, 0, 128, 12, SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(4, 2);
    display.print("ALARM CLOCK");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

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
  static unsigned long lastEncoderTime = 0;
  int currentClkState = digitalRead(ENCODER_CLK);
  int delta = 0;
  if (currentClkState != lastClkState) {
    lastClkState = currentClkState;
    if (currentClkState == LOW) {
      if (millis() - lastEncoderTime < 3) return 0; // Debounce
      lastEncoderTime = millis();
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

  preferences.end();
  esp_deep_sleep_start();
}

void playTick() {
  if (systemVolume == 0) return;
  int freq = (systemVolume == 1) ? 1000 : (systemVolume == 2) ? 2000 : 2400;
  tone(BUZZER_PIN, freq, 8);
}

void playSelectTone() {
  if (systemVolume == 0) return;
  tone(BUZZER_PIN, (systemVolume == 1) ? 1600 : 2200, 80);
}

void playCancelTone() {
  if (systemVolume == 0) return;
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
  display.setTextSize(1); // Fix: Reset font size when returning from submenus!
  
  const char* options[5] = {"Set Time", "Set Date", "Time Format", "Volume", "Brightness"};
  int midY = 32;
  int spacing = 14;
  extern int menuScrollAnimY;

  // 1. Draw FIXED selection box
  display.fillRect(0, midY - 3, 128, 13, SSD1306_WHITE);

  // 2. Draw sliding text in INVERSE mode
  display.setTextColor(SSD1306_INVERSE);

  for (int offset = -2; offset <= 2; offset++) {
    int optIdx = (settingsMenuSelect + offset + 5) % 5;
    int drawY = midY + (offset * spacing) + menuScrollAnimY;
    
    display.setCursor(10, drawY - 2);
    display.print(options[optIdx]);
    
    // Status indicators
    if (optIdx == 2) {
      display.setCursor(90, drawY - 2);
      display.print(use12HourFormat ? "12-HR" : "24-HR");
    } else if (optIdx == 3) {
      int miniBarX = 90;
      int miniBarY = drawY - 2;
      display.drawRect(miniBarX, miniBarY, 30, 7, SSD1306_INVERSE);
      int fillVal = (systemVolume * 26) / 3;
      if (fillVal > 0) display.fillRect(miniBarX + 2, miniBarY + 2, fillVal, 3, SSD1306_INVERSE);
    } else if (optIdx == 4) {
      int miniBarX = 90;
      int miniBarY = drawY - 2;
      display.drawRect(miniBarX, miniBarY, 30, 7, SSD1306_INVERSE);
      int fillVal = ((systemBrightness + 1) * 26) / 3;
      if (fillVal > 0) display.fillRect(miniBarX + 2, miniBarY + 2, fillVal, 3, SSD1306_INVERSE);
    }
  }

  // 3. Draw clipping mask and title
  display.fillRect(0, 0, 128, 12, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SETTINGS");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
  
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

  // 3. Battery Tick (non-blocking, only redraw if changed)
  if (now - lastBatteryCheckTime >= 2000) {
    lastBatteryCheckTime = now;
    updateBatteryMeasurements();
    if (globalBatteryPct != prevBatteryPct) {
      prevBatteryPct = globalBatteryPct;
      uiChanged = true;
    }
  }

  // 3.5. Pomodoro Timer Countdown Tick (RTC-based to avoid drift)
  if (currentOSState == OS_APP_POMODORO && currentPomoState == POMO_STATE_RUNNING) {
    // Track start reference
    if (pomoStartEpoch == 0) {
      struct timeval ptv; gettimeofday(&ptv, NULL);
      pomoStartEpoch = ptv.tv_sec;
      pomoStartLeft = pomoLeftSeconds;
    }
    struct timeval ptv; gettimeofday(&ptv, NULL);
    int elapsed = (int)(ptv.tv_sec - pomoStartEpoch);
    int newLeft = pomoStartLeft - elapsed;
    if (newLeft < 0) newLeft = 0;
    if (newLeft != pomoLeftSeconds) {
      pomoLeftSeconds = newLeft;
      uiChanged = true;
    }
    if (pomoLeftSeconds <= 0) {
      pomoStartEpoch = 0; // Reset for next session
      currentPomoState = POMO_STATE_FINISHED;
      uiChanged = true;
      playFinishedMelody();
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
  if (currentOSState == OS_APP_READER) {
    if (currentReaderState == READER_PLAYING) {
      unsigned long baseDelay = 60000 / readerWpm;
      unsigned long wordDelay = baseDelay;

      // Soft-start ramp up for first 5 words of reading session
      if (sessionWordsRead < 5) {
        float speedFactor = 0.5f + (0.1f * sessionWordsRead); // 50%, 60%, 70%, 80%, 90%
        wordDelay = (unsigned long)(baseDelay / speedFactor);
      }

      // Punctuation pause bonus
      String curr = getCurrentWord(readerWordIndex);
      if (curr.length() > 0) {
        char last = curr.charAt(curr.length() - 1);
        if ((last == '"' || last == '\'' || last == ')' || last == ']') && curr.length() > 1) {
          last = curr.charAt(curr.length() - 2);
        }
        if (readerPuncMode != 2) {
          if (last == '.' || last == '!' || last == '?') {
            wordDelay = (wordDelay * (readerPuncMode == 1 ? 30 : 22)) / 10;
          } else if (last == ',' || last == ';' || last == ':') {
            wordDelay = (wordDelay * (readerPuncMode == 1 ? 20 : 15)) / 10;
          }
        }
        int len = curr.length();
        if (readerLongMode != 2 && len >= 6) {
          int bonusPct = (len - 5) * (readerLongMode == 1 ? 25 : 12); 
          wordDelay = wordDelay + (wordDelay * bonusPct) / 100;
        }
      }

      if (now - lastReaderWordTime >= wordDelay) {
        lastReaderWordTime = now;
        readerWordIndex++;
        sessionWordsRead++;
        if (readerWordIndex >= readerTotalWords) {
          currentReaderState = READER_FINISHED;
        }
        uiChanged = true;
      }
    } else if (currentReaderState == READER_BOOK_SELECT || currentReaderState == READER_CHAPTER_SELECT) {
      static unsigned long lastScrollTick = 0;
      if (now - lastScrollTick >= 200) { // 5 FPS marquee tick to keep rotary encoder 100% responsive
        lastScrollTick = now;
        uiChanged = true;
      }
    }
  }

  // 3.8 Menu Scroll Animation Tick
  if (menuScrollAnimY != 0) {
    static unsigned long lastAnimTick = 0;
    if (now - lastAnimTick >= 40) {
      lastAnimTick = now;
      int step = abs(menuScrollAnimY) / 3 + 2;
      if (menuScrollAnimY > 0) {
        menuScrollAnimY -= step;
        if (menuScrollAnimY < 0) menuScrollAnimY = 0;
      } else {
        menuScrollAnimY += step;
        if (menuScrollAnimY > 0) menuScrollAnimY = 0;
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
    menuScrollAnimY = 0; // Reset animation on state transition (#17)
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
        menuScrollAnimY = encoderDelta * 48; // Set horizontal animation offset for app menu
      }
      if (btnEvent == BUTTON_CLICK) {
        // Open Selected App
        menuScrollAnimY = 0; // Reset animation on state transition (#17)
        if (selectedAppIndex == 0) currentOSState = OS_APP_POMODORO;
        else if (selectedAppIndex == 1) currentOSState = OS_APP_ALARM;
        else if (selectedAppIndex == 2) currentOSState = OS_APP_SETTINGS;
        else if (selectedAppIndex == 3) {
          currentOSState = OS_APP_TETRIS;
          display.setRotation(3); // Inverted Portrait mode
        }
        else if (selectedAppIndex == 4) {
          currentOSState = OS_APP_READER;
          initSPIFFS(); // Initialize SPIFFS once on entry (#15)
          currentReaderState = READER_HOME;
          readerHomeOption = 0;
        }
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentOSState = OS_LAUNCHER; // Back to home
        playCancelTone();
      }
      break;

    // --- APPS (TimOS Applications) ---
    case OS_APP_POMODORO:
      handlePomodoroApp(encoderDelta, btnEvent);
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
    
    drawTopBar(); // Draw unified top bar on top of any app (except tetris/launcher)

    display.display();
    uiChanged = false;
  }
  
  // 8. Update LED status (breathing, flashing, solid)
  updateLED();

  delay(1);
}
