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

void saveReaderProgress() {
  String key = "spiffs_p_" + String(selectedBookIdx);
  preferences.putInt(key.c_str(), readerWordIndex);
}

void loadReaderProgress() {
  String key = "spiffs_p_" + String(selectedBookIdx);
  readerWordIndex = preferences.getInt(key.c_str(), 0);
}

void drawReaderApp() {
  initSPIFFS();

  if (currentReaderState == READER_BOOK_SELECT || currentReaderState == READER_CHAPTER_SELECT) {
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("SELECT BOOK");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    if (totalBooks == 0) {
      display.setCursor(4, 25);
      display.print("No .txt files in SPIFFS!");
      display.setCursor(4, 38);
      display.print("[Click] Read Sample");
    } else {
      int visibleTop = (selectedBookIdx / 3) * 3;
      for (int i = visibleTop; i < visibleTop + 3 && i < totalBooks; i++) {
        int slot = i - visibleTop;
        int y = 16 + (slot * 13);
        if (i == selectedBookIdx) {
          display.fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
          display.setTextColor(SSD1306_BLACK);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(2, y);
        display.print(bookList[i].name);
      }
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, 56);
    display.print("[Click] Open  [Hold] Exit");

  } else if (currentReaderState == READER_MENU) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (totalBooks > 0 && selectedBookIdx < totalBooks) {
      display.print(bookList[selectedBookIdx].name);
    } else {
      display.print("Sample Reader");
    }
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    display.setCursor(5, 16);
    display.print("Resume: ");
    display.print(readerWordIndex);
    display.print("/");
    display.print(readerTotalWords);

    display.setCursor(5, 30);
    display.print("WPM Speed:");
    display.setTextSize(2);
    display.setCursor(5, 40);
    display.print(readerWpm);
    display.setTextSize(1);
    display.setCursor(55, 47);
    display.print("wpm");

    display.setCursor(0, 56);
    display.print("[Click] Start [Hold] Back");

  } else if (currentReaderState == READER_PLAYING || currentReaderState == READER_PAUSED) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(readerWpm);
    display.print(" wpm");
    if (currentReaderState == READER_PAUSED) {
      display.setCursor(50, 0);
      display.print("[PAUSED]");
    }

    display.drawFastVLine(45, 18, 4, SSD1306_WHITE);
    display.drawFastVLine(45, 44, 4, SSD1306_WHITE);
    display.drawFastHLine(0, 22, 128, SSD1306_WHITE);
    display.drawFastHLine(0, 43, 128, SSD1306_WHITE);

    if (readerWordIndex < readerTotalWords) {
      String currWord = getCurrentWord(readerWordIndex);

      int len = currWord.length();
      int orp = 0;
      if (len > 1) orp = 1;
      if (len > 5) orp = 2;
      if (len > 9) orp = 3;

      int pivotX = 45;
      int ts = 2;
      int charWidth = 12;
      int currStartX = (pivotX - 6) - (orp * charWidth);
      int currEndX = currStartX + (len * charWidth);
      int currY = 26;

      if (currStartX < 0 || currEndX > 128) {
        ts = 1;
        charWidth = 6;
        currStartX = (pivotX - 3) - (orp * charWidth);
        currY = 30;
      }

      display.setTextWrap(false);
      display.setTextSize(ts);
      display.setCursor(currStartX, currY);
      display.print(currWord);

      if (ts == 2) {
        display.fillRect(pivotX - 7, currY - 2, 14, 19, SSD1306_INVERSE);
      } else {
        display.fillRect(pivotX - 4, currY - 2, 8, 11, SSD1306_INVERSE);
      }
      display.setTextWrap(true);
    }

    int progress = (readerTotalWords > 0) ? (readerWordIndex * 128) / readerTotalWords : 0;
    display.drawRect(0, 56, 128, 8, SSD1306_WHITE);
    display.fillRect(0, 56, progress, 8, SSD1306_WHITE);

  } else if (currentReaderState == READER_FINISHED) {
    display.setTextSize(2);
    display.setCursor(20, 15);
    display.print("FINISHED");
    display.setTextSize(1);
    display.setCursor(15, 40);
    display.print("Click to restart");
  }
}

void handleReaderApp(int encoderDelta, ButtonEvent btnEvent) {
  initSPIFFS();

  if (currentReaderState == READER_BOOK_SELECT || currentReaderState == READER_CHAPTER_SELECT) {
    if (totalBooks > 0 && encoderDelta != 0) {
      selectedBookIdx += encoderDelta;
      if (selectedBookIdx < 0) selectedBookIdx = totalBooks - 1;
      if (selectedBookIdx >= totalBooks) selectedBookIdx = 0;
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
      currentReaderState = READER_MENU;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentOSState = OS_APP_MENU;
      playCancelTone();
    }
  } else if (currentReaderState == READER_MENU) {
    if (encoderDelta != 0) {
      readerWpm += encoderDelta * 25;
      if (readerWpm < 50) readerWpm = 50;
      if (readerWpm > 1000) readerWpm = 1000;
    }
    if (btnEvent == BUTTON_CLICK) {
      currentReaderState = READER_PLAYING;
      lastReaderWordTime = millis();
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
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
