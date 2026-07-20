import re

# 1. Update TimOS.ino
with open('TimOS/TimOS.ino', 'r', encoding='utf-8') as f:
    timos = f.read()

# Add WiFi headers and global variables
wifi_globals = """
#include <WiFi.h>

String wifiSSID = "";
String wifiPassword = "";
int wifiSyncState = 0; // 0=idle, 1=connecting, 2=waiting for time, 3=success
unsigned long wifiTaskTick = 0;
String currentInputText = "";
int textInputCursor = 0;
int currentCharIdx = 0;
int wifiListSelect = 0;
int wifiScanCount = 0;
"""

timos = timos.replace('#include "esp_sleep.h"', '#include "esp_sleep.h"\n' + wifi_globals)

# Update enum SettingsState
old_enum = """enum SettingsState {
  SET_STATE_MENU,
  SET_STATE_TIME_HH,
  SET_STATE_TIME_MM,
  SET_STATE_DATE_DD,
  SET_STATE_DATE_MM,
  SET_STATE_DATE_YY,
  SET_STATE_VOLUME,
  SET_STATE_BRIGHTNESS
};"""

new_enum = """enum SettingsState {
  SET_STATE_MENU,
  SET_STATE_TIME_HH,
  SET_STATE_TIME_MM,
  SET_STATE_DATE_DD,
  SET_STATE_DATE_MM,
  SET_STATE_DATE_YY,
  SET_STATE_VOLUME,
  SET_STATE_BRIGHTNESS,
  SET_STATE_WIFI_LIST,
  SET_STATE_WIFI_PASS
};"""
timos = timos.replace(old_enum, new_enum)

# Update drawSettingsMenu
old_menu_array = 'const char* options[5] = {"Set Time", "Set Date", "Time Format", "Volume", "Brightness"};'
new_menu_array = 'const char* options[6] = {"Set Time", "Set Date", "Time Format", "Volume", "Brightness", "WiFi Time Sync"};'
timos = timos.replace(old_menu_array, new_menu_array)

timos = timos.replace('int optIdx = (settingsMenuSelect + offset + 5) % 5;', 'int optIdx = (settingsMenuSelect + offset + 6) % 6;')

# Add WiFi background check in loop()
# Find updateBatteryMeasurements(); in loop and add after it.
wifi_tick = """
  // WiFi Background Task
  if (wifiSyncState == 1 && (millis() - wifiTaskTick > 1000)) {
    wifiTaskTick = millis();
    if (WiFi.status() == WL_CONNECTED) {
      configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // Assuming UTC+6
      wifiSyncState = 2;
    } else if (millis() - wifiTaskTick > 30000) {
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      wifiSyncState = 0;
    }
  } else if (wifiSyncState == 2 && (millis() - wifiTaskTick > 1000)) {
    wifiTaskTick = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
      struct timeval tv;
      tv.tv_sec = mktime(&timeinfo);
      settimeofday(&tv, NULL);
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      wifiSyncState = 3;
    } else if (millis() - wifiTaskTick > 60000) {
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      wifiSyncState = 0;
    }
  }
"""
timos = timos.replace('  if (now - lastBatteryCheckTime >= 2000) {', wifi_tick + '\n  if (now - lastBatteryCheckTime >= 2000) {')

# Add trigger in setup()
trigger = """
  wifiSSID = preferences.getString("wifiSSID", "");
  wifiPassword = preferences.getString("wifiPWD", "");
  if (wifiSSID.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    wifiSyncState = 1;
    wifiTaskTick = millis();
  }
"""
timos = timos.replace('lastActivityTime = millis();', 'lastActivityTime = millis();\n' + trigger)

with open('TimOS/TimOS.ino', 'w', encoding='utf-8') as f:
    f.write(timos)


# 2. Update App_Settings.ino
with open('TimOS/App_Settings.ino', 'r', encoding='utf-8') as f:
    settings = f.read()

# Add draw options
draw_add = """
    case SET_STATE_WIFI_LIST:  drawWiFiList(); break;
    case SET_STATE_WIFI_PASS:  drawWiFiPass(); break;
"""
settings = settings.replace('case SET_STATE_BRIGHTNESS: drawSettingsBrightness(); break;', 'case SET_STATE_BRIGHTNESS: drawSettingsBrightness(); break;\n' + draw_add)

# Update menu select constraints and handler
settings = settings.replace('if (settingsMenuSelect < 0) settingsMenuSelect = 4;', 'if (settingsMenuSelect < 0) settingsMenuSelect = 5;')
settings = settings.replace('if (settingsMenuSelect > 4) settingsMenuSelect = 0;', 'if (settingsMenuSelect > 5) settingsMenuSelect = 0;')

# Update settings click
old_click = """} else if (settingsMenuSelect == 4) {
          currentSettingsState = SET_STATE_BRIGHTNESS;
        }"""
new_click = """} else if (settingsMenuSelect == 4) {
          currentSettingsState = SET_STATE_BRIGHTNESS;
        } else if (settingsMenuSelect == 5) {
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
          delay(100);
          wifiScanCount = WiFi.scanNetworks();
          wifiListSelect = 0;
          currentSettingsState = SET_STATE_WIFI_LIST;
        }"""
settings = settings.replace(old_click, new_click)

# Append handler logic and drawing functions to the end
wifi_logic = """
void drawWiFiList() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("SELECT WIFI");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  if (wifiScanCount == 0) {
    display.setCursor(4, 24);
    display.print("No networks found.");
    return;
  }
  if (wifiScanCount < 0) {
    display.setCursor(4, 24);
    display.print("Scanning...");
    return;
  }

  for (int i = 0; i < 3; i++) {
    int idx = wifiListSelect - 1 + i;
    int y = 20 + i * 14;
    if (idx >= 0 && idx < wifiScanCount) {
      if (idx == wifiListSelect) {
        display.fillRect(0, y - 2, 128, 13, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(4, y);
      String ssid = WiFi.SSID(idx);
      if (ssid.length() > 20) ssid = ssid.substring(0, 18) + "..";
      display.print(ssid);
    }
  }
  display.setTextColor(SSD1306_WHITE);
}

void drawWiFiPass() {
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("WIFI PASSWORD");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // Draw typed password
  display.setCursor(4, 24);
  String disp = currentInputText;
  if (disp.length() > 20) disp = disp.substring(disp.length() - 20);
  display.print(disp);

  // Draw current char selector
  extern char charSet[];
  display.fillRect(4, 40, 12, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(6, 44);
  display.print(charSet[currentCharIdx]);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(22, 44);
  display.print("<- Scroll / Click");
}

void handleWiFiStates(int encoderDelta, ButtonEvent btnEvent) {
  if (currentSettingsState == SET_STATE_WIFI_LIST) {
    if (encoderDelta != 0 && wifiScanCount > 0) {
      wifiListSelect += encoderDelta;
      if (wifiListSelect < 0) wifiListSelect = wifiScanCount - 1;
      if (wifiListSelect >= wifiScanCount) wifiListSelect = 0;
    }
    if (btnEvent == BUTTON_CLICK && wifiScanCount > 0) {
      wifiSSID = WiFi.SSID(wifiListSelect);
      currentInputText = preferences.getString("wifiPWD", ""); // load previous pass if any
      currentCharIdx = 0;
      currentSettingsState = SET_STATE_WIFI_PASS;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentSettingsState = SET_STATE_MENU;
      playCancelTone();
    }
  } else if (currentSettingsState == SET_STATE_WIFI_PASS) {
    extern char charSet[];
    if (encoderDelta != 0) {
      currentCharIdx += encoderDelta;
      int len = strlen(charSet);
      if (currentCharIdx < 0) currentCharIdx = len - 1;
      if (currentCharIdx >= len) currentCharIdx = 0;
    }
    if (btnEvent == BUTTON_CLICK) {
      if (currentInputText.length() < 60) {
        currentInputText += charSet[currentCharIdx];
      }
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      wifiPassword = currentInputText;
      preferences.putString("wifiSSID", wifiSSID);
      preferences.putString("wifiPWD", wifiPassword);
      
      // Trigger background sync
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
      wifiSyncState = 1;
      wifiTaskTick = millis();
      
      currentSettingsState = SET_STATE_MENU;
      playCancelTone();
    }
  }
}
"""

# Inject handler logic
settings = settings.replace('case SET_STATE_BRIGHTNESS:', 'case SET_STATE_WIFI_LIST:\n    case SET_STATE_WIFI_PASS:\n      handleWiFiStates(encoderDelta, btnEvent);\n      break;\n\n    case SET_STATE_BRIGHTNESS:')
settings += '\n\n' + wifi_logic

with open('TimOS/App_Settings.ino', 'w', encoding='utf-8') as f:
    f.write(settings)

print("WiFi feature injected successfully.")
