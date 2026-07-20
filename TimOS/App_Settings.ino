void drawSettingsApp() {
  switch (currentSettingsState) {
    case SET_STATE_MENU:       drawSettingsMenu(); break;
    case SET_STATE_TIME_HH:
    case SET_STATE_TIME_MM:    drawSettingsTimeSet(); break;
    case SET_STATE_DATE_DD:
    case SET_STATE_DATE_MM:
    case SET_STATE_DATE_YY:    drawSettingsDateSet(); break;
    case SET_STATE_VOLUME:     drawSettingsVolume(); break;
    case SET_STATE_BRIGHTNESS: drawSettingsBrightness(); break;
    case SET_STATE_WIFI_LIST:  drawWiFiList(); break;
    case SET_STATE_WIFI_PASS:  drawWiFiPass(); break;
  }
}

void handleSettingsApp(int encoderDelta, ButtonEvent btnEvent) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm* timeinfo = localtime(&tv.tv_sec);

  switch (currentSettingsState) {
    case SET_STATE_MENU:
      if (encoderDelta != 0) {
        settingsMenuSelect += encoderDelta;
        if (settingsMenuSelect < 0) settingsMenuSelect = 5;
        if (settingsMenuSelect > 5) settingsMenuSelect = 0;
        extern int menuScrollAnimY;
        menuScrollAnimY = encoderDelta * 14;
      }
      if (btnEvent == BUTTON_CLICK) {
        playSelectTone();
        if (settingsMenuSelect == 0) {
          tempHour = timeinfo->tm_hour;
          tempMinute = timeinfo->tm_min;
          currentSettingsState = SET_STATE_TIME_HH;
        } else if (settingsMenuSelect == 1) {
          tempDay = timeinfo->tm_mday;
          tempMonth = timeinfo->tm_mon + 1;
          tempYear = timeinfo->tm_year + 1900;
          currentSettingsState = SET_STATE_DATE_DD;
        } else if (settingsMenuSelect == 2) {
          use12HourFormat = !use12HourFormat;
          preferences.putBool("12hr", use12HourFormat);
        } else if (settingsMenuSelect == 3) {
          currentSettingsState = SET_STATE_VOLUME;
        } else if (settingsMenuSelect == 4) {
          currentSettingsState = SET_STATE_BRIGHTNESS;
        } else if (settingsMenuSelect == 5) {
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
          delay(100);
          wifiScanCount = WiFi.scanNetworks();
          wifiListSelect = 0;
          currentSettingsState = SET_STATE_WIFI_LIST;
        }
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentOSState = OS_APP_MENU;
        playCancelTone();
      }
      break;

    case SET_STATE_TIME_HH:
      if (encoderDelta != 0) {
        tempHour += encoderDelta;
        if (tempHour < 0) tempHour = 23;
        if (tempHour > 23) tempHour = 0;
      }
      if (btnEvent == BUTTON_CLICK) {
        currentSettingsState = SET_STATE_TIME_MM;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_MENU;
        playCancelTone();
      }
      break;

    case SET_STATE_TIME_MM:
      if (encoderDelta != 0) {
        tempMinute += encoderDelta;
        if (tempMinute < 0) tempMinute = 59;
        if (tempMinute > 59) tempMinute = 0;
      }
      if (btnEvent == BUTTON_CLICK) {
        timeinfo->tm_hour = tempHour;
        timeinfo->tm_min = tempMinute;
        timeinfo->tm_sec = 0;
        tv.tv_sec = mktime(timeinfo);
        settimeofday(&tv, NULL);
        
        currentSettingsState = SET_STATE_MENU;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_TIME_HH;
        playCancelTone();
      }
      break;

    case SET_STATE_DATE_DD:
      if (encoderDelta != 0) {
        tempDay += encoderDelta;
        if (tempDay < 1) tempDay = 31;
        if (tempDay > 31) tempDay = 1;
      }
      if (btnEvent == BUTTON_CLICK) {
        currentSettingsState = SET_STATE_DATE_MM;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_MENU;
        playCancelTone();
      }
      break;

    case SET_STATE_DATE_MM:
      if (encoderDelta != 0) {
        tempMonth += encoderDelta;
        if (tempMonth < 1) tempMonth = 12;
        if (tempMonth > 12) tempMonth = 1;
      }
      if (btnEvent == BUTTON_CLICK) {
        currentSettingsState = SET_STATE_DATE_YY;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_DATE_DD;
        playCancelTone();
      }
      break;

    case SET_STATE_DATE_YY:
      if (encoderDelta != 0) {
        tempYear += encoderDelta;
        if (tempYear < 2020) tempYear = 2099;
        if (tempYear > 2099) tempYear = 2020;
      }
      if (btnEvent == BUTTON_CLICK) {
        timeinfo->tm_mday = tempDay;
        timeinfo->tm_mon = tempMonth - 1;
        timeinfo->tm_year = tempYear - 1900;
        tv.tv_sec = mktime(timeinfo);
        settimeofday(&tv, NULL);

        currentSettingsState = SET_STATE_MENU;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_DATE_MM;
        playCancelTone();
      }
      break;

    case SET_STATE_VOLUME:
      if (encoderDelta != 0) {
        systemVolume += encoderDelta;
        if (systemVolume < 0) systemVolume = 3;
        if (systemVolume > 3) systemVolume = 0;
        preferences.putInt("vol", systemVolume);
      }
      if (btnEvent == BUTTON_CLICK) {
        currentSettingsState = SET_STATE_MENU;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_MENU;
        playCancelTone();
      }
      break;

    case SET_STATE_WIFI_LIST:
    case SET_STATE_WIFI_PASS:
      handleWiFiStates(encoderDelta, btnEvent);
      break;

    case SET_STATE_BRIGHTNESS:
      if (encoderDelta != 0) {
        systemBrightness += encoderDelta;
        if (systemBrightness < 0) systemBrightness = 2;
        if (systemBrightness > 2) systemBrightness = 0;
        setDisplayBrightness(systemBrightness);
        preferences.putInt("bright", systemBrightness);
      }
      if (btnEvent == BUTTON_CLICK) {
        currentSettingsState = SET_STATE_MENU;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentSettingsState = SET_STATE_MENU;
        playCancelTone();
      }
      break;
  }
}




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
      if (wifiSSID == preferences.getString("wifiSSID", "")) {
        currentInputText = preferences.getString("wifiPWD", "");
      } else {
        currentInputText = "";
      }
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
      if (charSet[currentCharIdx] == '<') {
        if (currentInputText.length() > 0) {
          currentInputText.remove(currentInputText.length() - 1);
        }
      } else if (currentInputText.length() < 60) {
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
