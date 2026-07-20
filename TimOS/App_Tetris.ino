void drawTetrisApp();
void handleAlarmApp(int encoderDelta, ButtonEvent btnEvent);
void handleSettingsApp(int encoderDelta, ButtonEvent btnEvent);
void handleTetrisApp(int encoderDelta, ButtonEvent btnEvent);
void handleReaderApp(int encoderDelta, ButtonEvent btnEvent);
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
  setDisplayBrightness(systemBrightness); // Apply initial brightness level
  display.setTextColor(SSD1306_WHITE);
  
  updateBatteryMeasurements();
  lastActivityTime = millis();

  // Initialize Reader Text
  String fullText = String(sampleReaderText);
  int startIdx = 0;
  int endIdx = fullText.indexOf(' ');
  while (endIdx != -1 && readerTotalWords < MAX_READER_WORDS) {
    readerWords[readerTotalWords++] = fullText.substring(startIdx, endIdx);
    startIdx = endIdx + 1;
    endIdx = fullText.indexOf(' ', startIdx);
  }
  if (startIdx < fullText.length() && readerTotalWords < MAX_READER_WORDS) {
    readerWords[readerTotalWords++] = fullText.substring(startIdx);
  }

  // Show Boot Splash Screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(34, 24);
  display.print("TimOS");
  display.display();
  delay(1000);
}

void handleTetrisApp(int encoderDelta, ButtonEvent btnEvent) {
  if (currentTetrisState == TETRIS_MENU) {
    if (btnEvent == BUTTON_CLICK) {
      memset(tetrisBoard, 0, sizeof(tetrisBoard));
      tetrisScore = 0;
      tetrisSpeed = 500;
      currentTetrisState = TETRIS_PLAYING;
      spawnTetrisPiece();
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentOSState = OS_APP_MENU;
      display.setRotation(0);
      playCancelTone();
    }
  }
  else if (currentTetrisState == TETRIS_GAMEOVER) {
    if (btnEvent == BUTTON_CLICK || btnEvent == BUTTON_LONG_PRESS) {
      currentTetrisState = TETRIS_MENU;
      playCancelTone();
    }
  }
  else if (currentTetrisState == TETRIS_PLAYING) {
    if (encoderDelta != 0) {
      int newX = currentPieceX + encoderDelta;
      if (!checkTetrisCollision(currentPieceType, currentPieceRot, newX, currentPieceY)) {
        currentPieceX = newX;
      }
    }
    if (btnEvent == BUTTON_CLICK) {
      int newRot = (currentPieceRot + 1) % 4;
      if (!checkTetrisCollision(currentPieceType, newRot, currentPieceX, currentPieceY)) {
        currentPieceRot = newRot;
        playTick();
      }
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentTetrisState = TETRIS_MENU;
      playCancelTone();
    }
  }
}

bool checkTetrisCollision(int pType, int pRot, int pX, int pY) {
  uint16_t shape = tetrominoes[pType][pRot];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (shape & (1 << (15 - (r * 4 + c)))) {
        int boardX = pX + c;
        int boardY = pY + r;
        if (boardX < 0 || boardX >= TETRIS_COLS) return true;
        if (boardY >= TETRIS_ROWS) return true;
        if (boardY >= 0) {
          if (tetrisBoard[boardY] & (1 << boardX)) return true;
        }
      }
    }
  }
  return false;
}

void lockTetrisPiece() {
  uint16_t shape = tetrominoes[currentPieceType][currentPieceRot];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (shape & (1 << (15 - (r * 4 + c)))) {
        int boardY = currentPieceY + r;
        int boardX = currentPieceX + c;
        if (boardY >= 0 && boardY < TETRIS_ROWS) {
          tetrisBoard[boardY] |= (1 << boardX);
        }
      }
    }
  }
  playTick();

  int linesCleared = 0;
  for (int r = TETRIS_ROWS - 1; r >= 0; r--) {
    if ((tetrisBoard[r] & 0x3FF) == 0x3FF) {
      linesCleared++;
      for (int k = r; k > 0; k--) {
        tetrisBoard[k] = tetrisBoard[k - 1];
      }
      tetrisBoard[0] = 0;
      r++;
    }
  }

  if (linesCleared > 0) {
    tetrisScore += (linesCleared * linesCleared * 10);
    tetrisSpeed = max(100, 500 - (tetrisScore));
    playSelectTone();
  }

  if (currentTetrisState != TETRIS_GAMEOVER) {
    spawnTetrisPiece();
  }
}

void spawnTetrisPiece() {
  currentPieceType = random(0, 7);
  currentPieceRot = 0;
  currentPieceX = 3;
  currentPieceY = -2;
  if (checkTetrisCollision(currentPieceType, currentPieceRot, currentPieceX, currentPieceY)) {
    currentTetrisState = TETRIS_GAMEOVER;
    playCancelTone();
  }
}

