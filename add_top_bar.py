import re

timos_path = 'TimOS/TimOS.ino'

with open(timos_path, 'r', encoding='utf-8') as f:
    timos = f.read()

top_bar_code = """
void drawTopBar() {
  if (currentOSState == OS_APP_TETRIS || currentOSState == OS_LAUNCHER) return;
  
  // Clear the top area so it overlays over any app's old top bar
  display.fillRect(0, 0, 128, 13, SSD1306_BLACK);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  
  if (currentOSState == OS_APP_MENU) display.print("APPS");
  else if (currentOSState == OS_APP_POMODORO) display.print("POMODORO");
  else if (currentOSState == OS_APP_ALARM) display.print("ALARM");
  else if (currentOSState == OS_APP_SETTINGS) display.print("SETTINGS");
  else if (currentOSState == OS_APP_READER) {
    if (currentReaderState == READER_HOME) display.print("READER");
    else if (currentReaderState == READER_BOOK_SELECT) display.print("SELECT BOOK");
    else if (currentReaderState == READER_SETTINGS) display.print("READER SETTINGS");
    else display.print("READER");
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
"""

# Insert drawTopBar right after drawBatteryStatus
timos = timos.replace('}\n\n\n/* ========================================================================== */\n/*                              ALARM APPLICATION                             */', '}\n' + top_bar_code + '\n\n/* ========================================================================== */\n/*                              ALARM APPLICATION                             */')


# Modify the loop UI drawing
old_loop_ui = """    // 7. Draw UI Layer
    if (uiChanged) {
      display.clearDisplay();
      if (currentOSState != OS_APP_TETRIS) drawBatteryStatus(); // Skip in Tetris rotated mode (#8)
      
      if (currentOSState == OS_LAUNCHER) {
        drawLauncher();
      } else if (currentOSState == OS_APP_MENU) {"""

new_loop_ui = """    // 7. Draw UI Layer
    if (uiChanged) {
      display.clearDisplay();
      if (currentOSState == OS_LAUNCHER) drawBatteryStatus();
      
      if (currentOSState == OS_LAUNCHER) {
        drawLauncher();
      } else if (currentOSState == OS_APP_MENU) {"""

timos = timos.replace(old_loop_ui, new_loop_ui)


old_loop_end = """      } else if (currentOSState == OS_APP_READER) {
        drawReaderApp();
      }

      display.display();
      uiChanged = false;
    }
  }
}"""

new_loop_end = """      } else if (currentOSState == OS_APP_READER) {
        drawReaderApp();
      }

      drawTopBar(); // Draw unified top bar on top of any app
      display.display();
      uiChanged = false;
    }
  }
}"""

timos = timos.replace(old_loop_end, new_loop_end)

# Also remove the specific black mask drawing from READER_BOOK_SELECT since topbar does it now.
# Wait, let's leave it in App_Reader.ino, it will just get overwritten by TopBar anyway! It's safer.

with open(timos_path, 'w', encoding='utf-8') as f:
    f.write(timos)

print("Unified Top Bar script generated successfully.")
