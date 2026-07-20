// TimOS Alarm App

void handleAlarmApp(int encoderDelta, ButtonEvent btnEvent) {
  if (alarmActive) {
    if (btnEvent == BUTTON_CLICK || btnEvent == BUTTON_LONG_PRESS) {
      alarmActive = false;
      noTone(BUZZER_PIN);
      currentOSState = OS_LAUNCHER;
      playCancelTone();
    }
    return;
  }

  switch (currentAlarmState) {
    case ALARM_STATE_MENU:
      if (encoderDelta != 0) {
        alarmMenuSelect += encoderDelta;
        if (alarmMenuSelect < 0) alarmMenuSelect = 1;
        if (alarmMenuSelect > 1) alarmMenuSelect = 0;
        extern int menuScrollAnimY;
        menuScrollAnimY = encoderDelta * 14;
      }
      if (btnEvent == BUTTON_CLICK) {
        if (alarmMenuSelect == 0) { // Toggle status
          alarmEnabled = !alarmEnabled;
          preferences.putBool("alarmEn", alarmEnabled);
          playSelectTone();
        } else { // Enter Set Time UI
          tempAlarmHour = alarmHour;
          tempAlarmMinute = alarmMinute;
          currentAlarmState = ALARM_STATE_SET_HOUR;
          playSelectTone();
        }
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentOSState = OS_APP_MENU;
        playCancelTone();
      }
      break;

    case ALARM_STATE_SET_HOUR:
      if (encoderDelta != 0) {
        tempAlarmHour += encoderDelta;
        if (tempAlarmHour < 0) tempAlarmHour = 23;
        if (tempAlarmHour > 23) tempAlarmHour = 0;
      }
      if (btnEvent == BUTTON_CLICK) {
        currentAlarmState = ALARM_STATE_SET_MINUTE;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentAlarmState = ALARM_STATE_MENU;
        playCancelTone();
      }
      break;

    case ALARM_STATE_SET_MINUTE:
      if (encoderDelta != 0) {
        tempAlarmMinute += encoderDelta;
        if (tempAlarmMinute < 0) tempAlarmMinute = 59;
        if (tempAlarmMinute > 59) tempAlarmMinute = 0;
      }
      if (btnEvent == BUTTON_CLICK) {
        alarmHour = tempAlarmHour;
        alarmMinute = tempAlarmMinute;
        preferences.putInt("alarmHr", alarmHour);
        preferences.putInt("alarmMin", alarmMinute);
        alarmEnabled = true;
        preferences.putBool("alarmEn", alarmEnabled);
        currentAlarmState = ALARM_STATE_MENU;
        playSelectTone();
      }
      if (btnEvent == BUTTON_LONG_PRESS) {
        currentAlarmState = ALARM_STATE_SET_HOUR;
        playCancelTone();
      }
      break;
      
    case ALARM_STATE_RINGING:
      break;
  }
}
