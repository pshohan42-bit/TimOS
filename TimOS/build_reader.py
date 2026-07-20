import os

stories_dir = 'd:/Workspace/Pomodoro/Pomodoro simple 32c3/TimOS/data/exhalation'
output_file = 'd:/Workspace/Pomodoro/Pomodoro simple 32c3/TimOS/App_Reader.ino'

story_files = [
    ("Merchant & Alchemist", "01_Merchant_and_Alchemist.txt"),
    ("Exhalation", "02_Exhalation.txt"),
    ("What's Expected", "03_Whats_Expected_of_Us.txt"),
    ("Software Objects", "04_Lifecycle_of_Software.txt"),
    ("Dacey's Nanny", "05_Daceys_Automatic_Nanny.txt"),
    ("Truth of Fact", "06_Truth_of_Fact.txt"),
    ("The Great Silence", "07_The_Great_Silence.txt"),
    ("Omphalos", "08_Omphalos.txt"),
    ("Anxiety & Dizziness", "09_Anxiety_is_Dizziness.txt")
]

code = """// TimOS Speed Reader App with Book & Story Selector
#include <pgmspace.h>

"""

for i, (title, fname) in enumerate(story_files):
    path = os.path.join(stories_dir, fname)
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()
    # Clean up non-ASCII quotes and multi-spaces
    text = text.replace('“', '"').replace('”', '"').replace('’', "'").replace('—', ' - ')
    text = ' '.join(text.split())
    text_escaped = text.replace('\\', '\\\\').replace('"', '\\"')
    code += f'static const char story_{i+1}_data[] PROGMEM = "{text_escaped}";\n\n'

code += """
struct StoryInfo {
  const char* title;
  const char* data;
};

static const StoryInfo exhalationStories[9] = {
  {"1. Merchant & Alch.", story_1_data},
  {"2. Exhalation", story_2_data},
  {"3. What's Expected", story_3_data},
  {"4. Software Objects", story_4_data},
  {"5. Dacey's Nanny", story_5_data},
  {"6. Truth of Fact", story_6_data},
  {"7. Great Silence", story_7_data},
  {"8. Omphalos", story_8_data},
  {"9. Anxiety & Dizz.", story_9_data}
};

static const char* bookTitles[2] = {"1. Exhalation", "2. TimOS Guide"};
int selectedBookIdx = 0;
int selectedStoryIdx = 0;

// Helper to get word by index from PROGMEM string
String getWordFromProgmem(const char* text, int targetIndex, int &totalCount) {
  int count = 0;
  int i = 0;
  String result = "";
  
  while (true) {
    char c = pgm_read_byte(text + i);
    if (c == '\\0') break;
    
    while (c != '\\0' && (c == ' ' || c == '\\n' || c == '\\r' || c == '\\t')) {
      i++;
      c = pgm_read_byte(text + i);
    }
    if (c == '\\0') break;
    
    int wordStart = i;
    while (c != '\\0' && c != ' ' && c != '\\n' && c != '\\r' && c != '\\t') {
      i++;
      c = pgm_read_byte(text + i);
    }
    int wordLen = i - wordStart;
    
    if (count == targetIndex) {
      for (int k = 0; k < wordLen; k++) {
        result += (char)pgm_read_byte(text + wordStart + k);
      }
    }
    count++;
  }
  totalCount = count;
  return result;
}

// Count total words in PROGMEM string
int countWordsInProgmem(const char* text) {
  int count = 0;
  int i = 0;
  while (true) {
    char c = pgm_read_byte(text + i);
    if (c == '\\0') break;
    while (c != '\\0' && (c == ' ' || c == '\\n' || c == '\\r' || c == '\\t')) {
      i++;
      c = pgm_read_byte(text + i);
    }
    if (c == '\\0') break;
    while (c != '\\0' && c != ' ' && c != '\\n' && c != '\\r' && c != '\\t') {
      i++;
      c = pgm_read_byte(text + i);
    }
    count++;
  }
  return count;
}

void saveReaderProgress() {
  String key = "r_b" + String(selectedBookIdx) + "s" + String(selectedStoryIdx);
  preferences.putInt(key.c_str(), readerWordIndex);
}

void loadReaderProgress() {
  String key = "r_b" + String(selectedBookIdx) + "s" + String(selectedStoryIdx);
  readerWordIndex = preferences.getInt(key.c_str(), 0);
}

const char* getCurrentStoryData() {
  if (selectedBookIdx == 0) {
    return exhalationStories[selectedStoryIdx].data;
  } else {
    return sampleReaderText;
  }
}

void drawReaderApp() {
  if (currentReaderState == READER_BOOK_SELECT) {
    display.setTextSize(1);
    display.setCursor(30, 0);
    display.print("SELECT BOOK");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    
    for (int i = 0; i < 2; i++) {
      int y = 20 + (i * 18);
      if (i == selectedBookIdx) {
        display.fillRect(0, y - 2, 128, 14, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(5, y);
      display.print(bookTitles[i]);
    }
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("[Click] Open  [Hold] Exit");

  } else if (currentReaderState == READER_CHAPTER_SELECT) {
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("SELECT STORY");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    
    int totalStories = (selectedBookIdx == 0) ? 9 : 1;
    int visibleTop = (selectedStoryIdx / 3) * 3;
    
    for (int i = visibleTop; i < visibleTop + 3 && i < totalStories; i++) {
      int slot = i - visibleTop;
      int y = 16 + (slot * 13);
      if (i == selectedStoryIdx) {
        display.fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(2, y);
      if (selectedBookIdx == 0) display.print(exhalationStories[i].title);
      else display.print("1. TimOS Overview");
    }
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print("[Click] Select [Hold] Back");

  } else if (currentReaderState == READER_MENU) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (selectedBookIdx == 0) display.print(exhalationStories[selectedStoryIdx].title);
    else display.print("TimOS Overview");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    
    display.setCursor(5, 16);
    display.print("Resume Word: ");
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
      int dummyCount = 0;
      String currWord = getWordFromProgmem(getCurrentStoryData(), readerWordIndex, dummyCount);

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
  if (currentReaderState == READER_BOOK_SELECT) {
    if (encoderDelta != 0) {
      selectedBookIdx += encoderDelta;
      if (selectedBookIdx < 0) selectedBookIdx = 1;
      if (selectedBookIdx > 1) selectedBookIdx = 0;
    }
    if (btnEvent == BUTTON_CLICK) {
      selectedStoryIdx = 0;
      currentReaderState = READER_CHAPTER_SELECT;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentOSState = OS_APP_MENU;
      playCancelTone();
    }
  } else if (currentReaderState == READER_CHAPTER_SELECT) {
    int maxStories = (selectedBookIdx == 0) ? 9 : 1;
    if (encoderDelta != 0) {
      selectedStoryIdx += encoderDelta;
      if (selectedStoryIdx < 0) selectedStoryIdx = maxStories - 1;
      if (selectedStoryIdx >= maxStories) selectedStoryIdx = 0;
    }
    if (btnEvent == BUTTON_CLICK) {
      readerTotalWords = countWordsInProgmem(getCurrentStoryData());
      loadReaderProgress();
      currentReaderState = READER_MENU;
      playSelectTone();
    }
    if (btnEvent == BUTTON_LONG_PRESS) {
      currentReaderState = READER_BOOK_SELECT;
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
      currentReaderState = READER_CHAPTER_SELECT;
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
      currentReaderState = READER_CHAPTER_SELECT;
      playCancelTone();
    }
  }
}
"""

with open(output_file, 'w', encoding='utf-8') as f:
    f.write(code)

print("Generated App_Reader.ino with all 9 Exhalation stories embedded!")
