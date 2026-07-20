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
        if (settingsMenuSelect < 0) settingsMenuSelect = 4;
        if (settingsMenuSelect > 4) settingsMenuSelect = 0;
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

