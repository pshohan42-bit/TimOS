// TimOS Dynamic SPIFFS Speed Reader App
#include <FS.h>
#include <SPIFFS.h>

struct BookItem {
  String path;
  String name;
  int wordCount;
};

#define MAX_BOOKS 20
static BookItem bookList[MAX_BOOKS];
static int totalBooks = 0;
static int selectedBookIdx = 0;
static bool spiffsInitialized = false;
static int readerMenuOption = 0; // 0: Resume/Start, 1: Start Over, 2: Speed WPM
static bool isAdjustingWpm = false;

// We cache word offsets every 100 words to make seeking extremely fast
#define INDEX_INTERVAL 100
#define MAX_INDEX_ENTRIES 1000 // Supports up to 100,000 words
static uint32_t currentBookWordOffsets[MAX_INDEX_ENTRIES];

static String cachedCurrentWord = "";
static int cachedWordIndex = -1;

// Fallback embedded text if no SPIFFS text files are found
static const char fallbackText[] PROGMEM = "Welcome to TimOS Speed Reader! Upload your .txt files to SPIFFS flash storage to read any book dynamically.";

void initSPIFFS() {
  if (spiffsInitialized) return;
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS Mount Failed");
    spiffsInitialized = false;
    totalBooks = 0;
    return;
  }
  spiffsInitialized = true;
  rescanBooks();
}

void rescanBooks() {
  totalBooks = 0;
  File root = SPIFFS.open("/");
  if (!root) return;
  File file = root.openNextFile();
  while (file && totalBooks < MAX_BOOKS) {
    String fname = String(file.name());
    if (!file.isDirectory() && (fname.endsWith(".txt") || fname.endsWith(".TXT"))) {
      bookList[totalBooks].path = (fname.startsWith("/")) ? fname : "/" + fname;
      String disp = fname;
      if (disp.startsWith("/")) disp = disp.substring(1);
      if (disp.endsWith(".txt") || disp.endsWith(".TXT")) disp = disp.substring(0, disp.length() - 4);
      bookList[totalBooks].name = disp;
      bookList[totalBooks].wordCount = 0; // We count words ONLY when selected to prevent hanging on startup!
      totalBooks++;
    }
    file = root.openNextFile();
  }
}

int countFileWordsAndBuildIndex(const char* path) {
  File f = SPIFFS.open(path, "r");
  if (!f) return 0;
  
  int count = 0;
  bool inWord = false;
  uint32_t byteOffset = 0;
  
  // Fast buffered read
  uint8_t buf[512];
  while (f.available()) {
    size_t len = f.read(buf, sizeof(buf));
    for (size_t i = 0; i < len; i++) {
      char c = (char)buf[i];
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        if (inWord) {
          count++;
          inWord = false;
        }
      } else {
        if (!inWord) {
          inWord = true;
          if (count % INDEX_INTERVAL == 0) {
            int entry = count / INDEX_INTERVAL;
            if (entry < MAX_INDEX_ENTRIES) {
              currentBookWordOffsets[entry] = byteOffset;
            }
          }
        }
      }
      byteOffset++;
    }
  }
  if (inWord) count++;
  f.close();
  return count;
}

String getWordFromFileFast(const char* path, int targetIdx) {
  if (targetIdx == cachedWordIndex) return cachedCurrentWord;
  
  File f = SPIFFS.open(path, "r");
  if (!f) return "";
  
  int entry = targetIdx / INDEX_INTERVAL;
  if (entry >= MAX_INDEX_ENTRIES) entry = MAX_INDEX_ENTRIES - 1;
  uint32_t startOffset = currentBookWordOffsets[entry];
  int currentWordCount = entry * INDEX_INTERVAL;
  
  f.seek(startOffset, SeekSet);
  
  String word = "";
  bool inWord = false;
  
  while (f.available()) {
    char c = (char)f.read();
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      if (inWord) {
        if (currentWordCount == targetIdx) {
          f.close();
          cachedWordIndex = targetIdx;
          cachedCurrentWord = word;
          return word;
        }
        currentWordCount++;
        word = "";
        inWord = false;
      }
    } else {
      if (!inWord) inWord = true;
      if (currentWordCount == targetIdx) {
        word += c;
      }
    }
  }
  
  if (inWord && currentWordCount == targetIdx) {
    f.close();
    cachedWordIndex = targetIdx;
    cachedCurrentWord = word;
    return word;
  }
  f.close();
  return "";
}

String getWordFromFallback(int targetIdx) {
  if (targetIdx == cachedWordIndex) return cachedCurrentWord;
  int count = 0;
  int i = 0;
  String result = "";
  while (true) {
    char c = pgm_read_byte(fallbackText + i);
    if (c == '\0') break;
    while (c != '\0' && (c == ' ' || c == '\n' || c == '\r' || c == '\t')) {
      i++;
      c = pgm_read_byte(fallbackText + i);
    }
    if (c == '\0') break;
    int wordStart = i;
    while (c != '\0' && c != ' ' && c != '\n' && c != '\r' && c != '\t') {
      i++;
      c = pgm_read_byte(fallbackText + i);
    }
    if (count == targetIdx) {
      for (int k = 0; k < (i - wordStart); k++) {
        result += (char)pgm_read_byte(fallbackText + wordStart + k);
      }
      cachedWordIndex = targetIdx;
      cachedCurrentWord = result;
      return result;
    }
    count++;
  }
  return "";
}

String getCurrentWord(int targetIdx) {
  if (totalBooks > 0 && selectedBookIdx < totalBooks) {
    return getWordFromFileFast(bookList[selectedBookIdx].path.c_str(), targetIdx);
  } else {
    return getWordFromFallback(targetIdx);
  }
}

int findSentenceStart(int targetIdx) {
  if (targetIdx <= 0) return 0;
  // Look back up to 40 words for a sentence boundary ('.', '!', '?')
  for (int i = targetIdx - 1; i >= 0 && i >= targetIdx - 40; i--) {
    String w = getCurrentWord(i);
    if (w.length() > 0) {
      char last = w.charAt(w.length() - 1);
      if ((last == '"' || last == '\'' || last == ')' || last == ']') && w.length() > 1) {
        last = w.charAt(w.length() - 2);
      }
      if (last == '.' || last == '!' || last == '?') {
        return i + 1; // Sentence starts at the following word
      }
    }
  }
  return targetIdx;
}

void saveReaderProgress() {
  String key = "spiffs_p_" + String(selectedBookIdx);
  preferences.putInt(key.c_str(), readerWordIndex);
}

void loadReaderProgress() {
  String key = "spiffs_p_" + String(selectedBookIdx);
  int savedIdx = preferences.getInt(key.c_str(), 0);
  readerWordIndex = findSentenceStart(savedIdx);
}

void drawReaderApp() {
  initSPIFFS();

  if (currentReaderState == READER_BOOK_SELECT || currentReaderState == READER_CHAPTER_SELECT) {
    if (totalBooks == 0) {
      display.setCursor(4, 25);
      display.print("No .txt files in SPIFFS!");
      display.setCursor(4, 38);
      display.print("Run upload_books.py");
    } else {
      display.setTextWrap(false);
      extern int menuScrollAnimY;
      int centerSlotY = 29;

      // 1. Draw FIXED selection box
      display.fillRect(0, centerSlotY - 1, 128, 13, SSD1306_WHITE);
      
      // 2. Draw sliding text in INVERSE mode (automatically turns black over the white box!)
      display.setTextColor(SSD1306_INVERSE);

      for (int offset = -2; offset <= 2; offset++) {
        int i = selectedBookIdx + offset;
        
        if (totalBooks >= 5) {
          while (i < 0) i += totalBooks;
          i = i % totalBooks;
        } else {
          if (i < 0 || i >= totalBooks) continue;
        }

        int y = centerSlotY + (offset * 15) + menuScrollAnimY;
        int nameWidth = bookList[i].name.length() * 6;
        int scrollX = 2;

        // Marquee for the selected center item
        if (offset == 0 && nameWidth > 124) {
          int overflowChars = (nameWidth - 110) / 6 + 1;
          int cycle = (millis() / 200) % (overflowChars + 6);
          if (cycle < 2) {
            scrollX = 2;
          } else if (cycle < 2 + overflowChars) {
            scrollX = 2 - ((cycle - 2) * 6);
          } else {
            scrollX = 2 - (overflowChars * 6);
          }
        }

        display.setCursor(scrollX, y);
        display.print(bookList[i].name);
      }
      display.setTextColor(SSD1306_WHITE);
      display.setTextWrap(true);
    }

    // 3. Draw clipping mask and title bar on top of the scrolling list!
    display.fillRect(0, 0, 128, 11, SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("SELECT BOOK");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  } else if (currentReaderState == READER_MENU) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setTextWrap(false);
    if (totalBooks > 0 && selectedBookIdx < totalBooks) {
      display.print(bookList[selectedBookIdx].name);
    } else {
      display.print("Sample Reader");
    }
    display.setTextWrap(true);
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Option 0: Resume / Start Reading
    int y0 = 16;
    if (readerMenuOption == 0) {
      display.fillRect(0, y0 - 1, 128, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(2, y0);
    if (readerWordIndex > 0) {
      display.print("Resume (w:");
      display.print(readerWordIndex);
      display.print("/");
      display.print(readerTotalWords);
      display.print(")");
    } else {
      display.print("Start Reading >");
    }

    // Option 1: Start Over
    int y1 = 29;
    if (readerMenuOption == 1) {
      display.fillRect(0, y1 - 1, 128, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(2, y1);
    display.print("Restart from Word 0");

    // Option 2: WPM Speed
    int y2 = 42;
    if (readerMenuOption == 2) {
      if (isAdjustingWpm) {
        display.fillRect(0, y2 - 1, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.drawRect(0, y2 - 1, 128, 11, SSD1306_WHITE); // Hollow highlight
        display.setTextColor(SSD1306_WHITE);
      }
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(2, y2);
    display.print("Speed: ");
    if (readerMenuOption == 2 && isAdjustingWpm && (millis() / 300) % 2 == 0) display.print("< ");
    display.print(readerWpm);
    if (readerMenuOption == 2 && isAdjustingWpm && (millis() / 300) % 2 == 0) display.print(" >");
    display.print(" wpm");

    display.setTextColor(SSD1306_WHITE);

  } else if (currentReaderState == READER_PLAYING || currentReaderState == READER_PAUSED) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(readerWpm);
    display.print(" wpm");
    if (currentReaderState == READER_PAUSED) {
      display.setCursor(50, 0);
      display.print("[PAUSED]");
    }

    // Upper and lower focus indication marks (NO horizontal lines)
    display.drawFastVLine(45, 18, 5, SSD1306_WHITE);
    display.drawFastVLine(45, 42, 5, SSD1306_WHITE);

    if (readerWordIndex < readerTotalWords) {
      String currWord = getCurrentWord(readerWordIndex);

      display.setTextWrap(false);
      int len = currWord.length();
      int orp = 0;
      if (len > 1) orp = 1;
      if (len > 5) orp = 2;
      if (len > 9) orp = 3;
      if (len > 13) orp = 4;

      int pivotX = 45;
      int charWidth, currY;

      if (len <= 10) {
        // Tier 1: Default Size 2 (12px wide, 16px tall) — big & bold
        charWidth = 12;
        currY = 24;
        int currStartX = (pivotX - 6) - (orp * charWidth);
        if (currStartX < 0) currStartX = 0;
        if (currStartX + (len * charWidth) > 128) currStartX = 128 - (len * charWidth);

        display.setFont(NULL);
        display.setTextSize(2);
        display.setCursor(currStartX, currY);
        display.print(currWord);
      } else if (len <= 16) {
        // Tier 2: CustomReaderFont (8px wide, ~11px tall) — clean medium
        charWidth = 8;
        currY = 34;
        int currStartX = (pivotX - 4) - (orp * charWidth);
        if (currStartX < 0) currStartX = 0;
        if (currStartX + (len * charWidth) > 128) currStartX = 128 - (len * charWidth);

        display.setFont(&CustomReaderFont);
        display.setTextSize(1);
        display.setCursor(currStartX, currY);
        display.print(currWord);
        display.setFont(NULL);
      } else {
        // Tier 3: Default Size 1 (6px wide, 8px tall) — compact fallback
        charWidth = 6;
        currY = 28;
        int currStartX = (pivotX - 3) - (orp * charWidth);
        if (currStartX < 0) currStartX = 0;
        if (currStartX + (len * charWidth) > 128) currStartX = 128 - (len * charWidth);

        display.setFont(NULL);
        display.setTextSize(1);
        display.setCursor(currStartX, currY);
        display.print(currWord);
      }

      display.setFont(NULL);
      display.setTextSize(1);
      display.setTextWrap(true);

    }

    int progress = (readerTotalWords > 0) ? (readerWordIndex * 128) / readerTotalWords : 0;
    display.drawRect(0, 56, 128, 8, SSD1306_WHITE);
    display.fillRect(0, 56, progress, 8, SSD1306_WHITE);

  } else if (currentReaderState == READER_FINISHED) {
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print("FINISHED");
  }
}

void handleReaderApp(int encoderDelta, ButtonEvent btnEvent) {
  initSPIFFS();

  if (currentReaderState == READER_BOOK_SELECT || currentReaderState == READER_CHAPTER_SELECT) {
    if (totalBooks > 0 && encoderDelta != 0) {
      selectedBookIdx += encoderDelta;
      if (selectedBookIdx < 0) selectedBookIdx = totalBooks - 1;
      if (selectedBookIdx >= totalBooks) selectedBookIdx = 0;
      extern int menuScrollAnimY;
      menuScrollAnimY = encoderDelta * 15;
    }
    if (btnEvent == BUTTON_CLICK) {
      if (totalBooks > 0) {
        // Only count words and build index when the book is ACTUALLY selected!
        if (bookList[selectedBookIdx].wordCount == 0) {
           display.clearDisplay();
           display.setCursor(10, 30);
           display.print("Loading book...");
           display.display();
           bookList[selectedBookIdx].wordCount = countFileWordsAndBuildIndex(bookList[selectedBookIdx].path.c_str());
        } else {
           // Rebuild index just in case
           countFileWordsAndBuildIndex(bookList[selectedBookIdx].path.c_str());
        }
        readerTotalWords = bookList[selectedBookIdx].wordCount;
      } else {
        readerTotalWords = 18; // Fallback word count
      }
      loadReaderProgress();
      cachedWordIndex = -1; // Reset cache
      readerMenuOption = 0; // Reset menu selection
      isAdjustingWpm = false;
      currentReaderState = READER_MENU;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentOSState = OS_APP_MENU;
      playCancelTone();
    }
  } else if (currentReaderState == READER_MENU) {
    if (encoderDelta != 0) {
      if (readerMenuOption == 2 && isAdjustingWpm) {
        // Adjust WPM speed directly when Speed item is highlighted and active
        readerWpm += encoderDelta * 25;
        if (readerWpm < 50) readerWpm = 50;
        if (readerWpm > 1000) readerWpm = 1000;
      } else {
        readerMenuOption += encoderDelta;
        if (readerMenuOption < 0) readerMenuOption = 2;
        if (readerMenuOption > 2) readerMenuOption = 0;
      }
    }
    if (btnEvent == BUTTON_CLICK) {
      if (readerMenuOption == 0) { // Resume / Start Reading
        currentReaderState = READER_PLAYING;
        sessionWordsRead = 0;
        lastReaderWordTime = millis();
        playSelectTone();
      } else if (readerMenuOption == 1) { // Restart from Beginning
        readerWordIndex = 0;
        saveReaderProgress();
        currentReaderState = READER_PLAYING;
        sessionWordsRead = 0;
        lastReaderWordTime = millis();
        playSelectTone();
      } else if (readerMenuOption == 2) { // Toggle WPM adjustment mode
        isAdjustingWpm = !isAdjustingWpm;
        playSelectTone();
      }
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      isAdjustingWpm = false;
      currentReaderState = READER_BOOK_SELECT;
      playCancelTone();
    }
  } else if (currentReaderState == READER_PLAYING) {
    if (encoderDelta != 0) {
      readerWpm += encoderDelta * 10;
      if (readerWpm < 50) readerWpm = 50;
      if (readerWpm > 1000) readerWpm = 1000;
    }
    if (btnEvent == BUTTON_CLICK) {
      saveReaderProgress();
      currentReaderState = READER_PAUSED;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      saveReaderProgress();
      currentReaderState = READER_MENU;
      playCancelTone();
    }
  } else if (currentReaderState == READER_PAUSED) {
    if (encoderDelta != 0) {
      readerWordIndex += encoderDelta;
      if (readerWordIndex < 0) readerWordIndex = 0;
      if (readerWordIndex >= readerTotalWords) readerWordIndex = readerTotalWords - 1;
      saveReaderProgress();
    }
    if (btnEvent == BUTTON_CLICK) {
      currentReaderState = READER_PLAYING;
      sessionWordsRead = 0;
      lastReaderWordTime = millis();
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      saveReaderProgress();
      currentReaderState = READER_MENU;
      playCancelTone();
    }
  } else if (currentReaderState == READER_FINISHED) {
    if (btnEvent == BUTTON_CLICK) {
      readerWordIndex = 0;
      saveReaderProgress();
      currentReaderState = READER_MENU;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentReaderState = READER_BOOK_SELECT;
      playCancelTone();
    }
  }
}
