// TimOS Pomodoro App

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

void handlePomodoroApp(int encoderDelta, ButtonEvent btnEvent) {
if (currentPomoState == POMO_STATE_MENU) {
  if (encoderDelta != 0) {
    selectedPomoMode += encoderDelta;
    if (selectedPomoMode < 0) selectedPomoMode = 3;
    if (selectedPomoMode > 3) selectedPomoMode = 0;
    menuScrollAnimY = encoderDelta * 14;
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
    pomoStartEpoch = 0; // Reset RTC reference so it re-initializes on resume
    currentPomoState = POMO_STATE_PAUSED;
    playSelectTone();
  }
  if (btnEvent == BUTTON_LONG_PRESS) {
    pomoStartEpoch = 0; // Reset RTC reference
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
}
