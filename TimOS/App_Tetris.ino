// TimOS Tetris App

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
