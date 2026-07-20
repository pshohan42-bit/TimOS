void drawPomodoroApp();


/* ========================================================================== */
/*                             VISUALS & RENDERING                            */
/* ========================================================================== */

void drawLauncher() {
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

