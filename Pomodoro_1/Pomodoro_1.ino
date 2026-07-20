/**
 * ESP32C3-Pro OLED, Encoder, Buzzer & Direct Battery Monitor Test
 * Incorporates virtual power button logic using Deep Sleep and uses GPIO7 LED
 * as a software-controlled power indicator.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_sleep.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pins
#define ENCODER_CLK 0
#define ENCODER_DT  1
#define ENCODER_SW  3
#define BUZZER_PIN 10
#define LED_PIN     7
#define BATTERY_PIN 4

// Global Variables
int counter = 0;
int lastClkState = HIGH;

// Battery state variables (updated every 2 seconds)
float globalBatVolts = 0.0;
int globalBatteryPct = 100;
unsigned long lastBatteryCheckTime = 0;

// Virtual Power Button Timing Variables
unsigned long buttonPressStartTime = 0;
bool buttonReleasedSinceBoot = false; 

void setup() {
  Serial.begin(115200);
  
  // Setup GPIO7 LED as output and turn it ON to show system is awake
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); 

  pinMode(BUZZER_PIN, OUTPUT);

  // Setup Encoder Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  Serial.println("\n--- OLED, Encoder, Buzzer & Battery Test ---");

  // Determine wakeup reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("System woke up from Deep Sleep!");
  } else {
    Serial.println("System powered up from Reset/Power-on");
  }

  // Initialize I2C and OLED
  Wire.begin(5, 6);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed."));
    while (true) {
      // Fast blink to show error
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }

  // Horizontal flip command to un-mirror text
  display.ssd1306_command(0xA0); 

  display.clearDisplay();
  display.display();

  // Read initial CLK state
  lastClkState = digitalRead(ENCODER_CLK);
  
  // Power-on beep (ascending chime)
  tone(BUZZER_PIN, 1200, 80);
  delay(100);
  tone(BUZZER_PIN, 1800, 120);

  // Do an initial battery check
  updateBatteryMeasurements();
}

// Read battery metrics directly from OUT+ and store in global variables
void updateBatteryMeasurements() {
  int raw = analogRead(BATTERY_PIN);
  globalBatVolts = (raw / 4095.0) * 3.1 * 2.0;
  globalBatVolts = constrain(globalBatVolts, 3.2, 4.2);
  
  globalBatteryPct = (int)((globalBatVolts - 3.2) / 1.0 * 100.0);
  globalBatteryPct = constrain(globalBatteryPct, 0, 100);

  Serial.printf("Battery Check -> Direct Bat: %.2fV | Pct: %d%%\n", globalBatVolts, globalBatteryPct);
}

void drawBatteryStatus() {
  // Draw battery frame
  int batX = 104;
  int batY = 3;
  display.drawRect(batX, batY, 18, 9, SSD1306_WHITE);
  display.fillRect(batX + 18, batY + 2, 2, 5, SSD1306_WHITE); // Battery tip

  char statusStr[8];
  sprintf(statusStr, "%d%%", globalBatteryPct);

  // Right-align status text relative to battery icon
  int textWidth = strlen(statusStr) * 6;
  display.setTextSize(1);
  display.setCursor(batX - textWidth - 4, batY + 1);
  display.print(statusStr);

  // Draw battery level fill bar
  int fillWidth = map(globalBatteryPct, 0, 100, 0, 14);
  if (fillWidth > 0) {
    display.fillRect(batX + 2, batY + 2, fillWidth, 5, SSD1306_WHITE);
  }
}

// Power off procedure
void goToSleep() {
  Serial.println("Shutting down... Entering Deep Sleep.");

  // Play descending shutdown chime
  tone(BUZZER_PIN, 1800, 100);
  delay(120);
  tone(BUZZER_PIN, 1200, 100);
  delay(120);
  tone(BUZZER_PIN, 800, 150);
  delay(200);
  noTone(BUZZER_PIN);

  // Turn off the OLED display completely to prevent screen burn-in and save power
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  display.clearDisplay();
  display.display();

  // Turn off the GPIO7 status LED during sleep to save battery power
  digitalWrite(LED_PIN, LOW);

  // Configure GPIO3 (ENCODER_SW) to wake up the chip when it pulls LOW (button press)
  esp_deep_sleep_enable_gpio_wakeup(1ULL << ENCODER_SW, ESP_GPIO_WAKEUP_GPIO_LOW);

  // Put chip to sleep
  esp_deep_sleep_start();
}

void loop() {
  bool changed = false;
  unsigned long now = millis();

  // 1. Check battery voltage every 2000 milliseconds (non-blocking)
  if (now - lastBatteryCheckTime >= 2000) {
    lastBatteryCheckTime = now;
    updateBatteryMeasurements();
    changed = true; // Trigger screen refresh
  }

  // 2. Read Rotary Encoder Rotation
  int currentClkState = digitalRead(ENCODER_CLK);
  if (currentClkState != lastClkState) {
    lastClkState = currentClkState;
    if (currentClkState == LOW) {
      if (digitalRead(ENCODER_DT) == HIGH) {
        counter++;
      } else {
        counter--;
      }
      changed = true;
      Serial.print("Rotation: ");
      Serial.println(counter);
      tone(BUZZER_PIN, 2200, 8);
    }
  }

  // 3. Read Switch (SW) Button Press
  int btnState = digitalRead(ENCODER_SW);

  // Boot safety check: the user must release the button first before we track holds
  if (btnState == HIGH) {
    buttonReleasedSinceBoot = true;
  }

  if (btnState == LOW) {
    // Only track hold-to-sleep if button has been released at least once since booting
    if (buttonReleasedSinceBoot) {
      if (buttonPressStartTime == 0) {
        buttonPressStartTime = now;
      } else if (now - buttonPressStartTime >= 2000) {
        // Held for 2 seconds -> Turn off!
        goToSleep();
      }
    }
  } else {
    // If button was pressed and released quickly (less than 2 seconds) -> standard reset
    if (buttonPressStartTime > 0 && (now - buttonPressStartTime < 2000)) {
      counter = 0;
      changed = true;
      Serial.println("Quick Press - Reset Counter!");
      
      tone(BUZZER_PIN, 1800, 80);
      
      // Blink the LED off briefly to show button press feedback, then turn back on
      digitalWrite(LED_PIN, LOW);
      delay(80);
      digitalWrite(LED_PIN, HIGH);
    }
    buttonPressStartTime = 0;
  }

  // 4. Update OLED Display
  if (changed) {
    display.clearDisplay();

    // Title
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 4);
    display.print("POMODORO");
    
    // Draw Battery Icon & Percentage
    drawBatteryStatus();
    display.drawFastHLine(0, 15, 128, SSD1306_WHITE);

    // Display counter value
    display.setTextSize(3);
    display.setCursor(30, 24);
    display.printf("%+03d", counter);

    // Help Text
    display.setTextSize(1);
    display.setCursor(10, 54);
    display.print("[Hold SW to Sleep]");

    display.display();
  }

  delay(1); // Ultra-short delay to maximize encoder polling frequency
}
