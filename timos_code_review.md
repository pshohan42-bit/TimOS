# TimOS Full Code Review

> [!NOTE]
> Reviewed: All 7 `.ino` files, 3 Python scripts, 1 custom font header.
> Total codebase: ~2,400 lines of C++ firmware + ~400 lines of Python tooling.

---

## 🔴 Critical Bugs (Will Cause Crashes or Data Corruption)

### 1. Dead Code: `build_reader.py` Will Overwrite `App_Reader.ino`
[build_reader.py](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/build_reader.py) is an **obsolete code generator** from the old PROGMEM-based reader. If anyone runs it, it will **completely destroy** the current SPIFFS-based `App_Reader.ino` with an incompatible old version (hardcoded story data, different function signatures, different `drawReaderApp()`).

**Risk:** Accidental data loss of the entire Reader app.
**Fix:** Delete `build_reader.py` entirely, or rename it to `build_reader_LEGACY.py.bak`.

---

### 2. Dead Global Variables Wasting 1.6KB RAM
[TimOS.ino:132-142](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L132-L142): The old embedded reader infrastructure is still alive in RAM:
```cpp
const char* sampleReaderText = "TimOS is a powerful...";  // ~370 bytes in RAM
String readerWords[MAX_READER_WORDS];                      // 100 String objects (~1.2KB)
```
The `readerWords[]` array is **populated in `setup()` at line 53-63** but **never used by the current SPIFFS reader**. The current reader uses `getCurrentWord()` which reads from SPIFFS files. This wastes **~1.6KB of precious ESP32-C3 RAM** for nothing.

**Fix:** Remove `sampleReaderText`, `readerWords[]`, `MAX_READER_WORDS`, and the word-splitting loop in `setup()`.

---

### 3. Preferences NVS Opened But Never Closed
[App_Tetris.ino:13](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Tetris.ino#L13): `preferences.begin("timos", false)` is called in `setup()` but `preferences.end()` is **never** called anywhere.

While ESP32 NVS handles this gracefully in practice, it holds an open handle forever. If a deep sleep → wake cycle occurs, `setup()` runs again and calls `begin()` on an already-open handle. The ESP-IDF NVS library silently handles double-open, but it's technically undefined behavior.

**Fix:** Call `preferences.end()` before `goToSleep()`, or at minimum document this is intentional.

---

### 4. `sampleReaderText` in RAM Instead of PROGMEM
[TimOS.ino:133](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L133): The fallback sample text is declared as `const char*` which places it in **RAM** on ESP32. The fallback text in `App_Reader.ino:28` correctly uses `PROGMEM`, but this duplicate in `TimOS.ino` does not.

**Fix:** If keeping the fallback, remove the `sampleReaderText` from `TimOS.ino` entirely (the one in `App_Reader.ino` is the one actually used).

---

### 5. `saveReaderProgress()` Called On Every Encoder Tick While Paused
[App_Reader.ino:521](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Reader.ino#L521): When the reader is paused and the user scrolls through words with the encoder, `saveReaderProgress()` fires on **every single encoder detent**. NVS flash has a limited write endurance (~100K cycles per sector). Rapid scrolling through a 30,000-word book could burn through hundreds of writes per session.

**Fix:** Only save progress when **leaving** the paused state (transitioning to PLAYING, MENU, or via long-press).

---

## 🟠 Significant Issues (Functional Problems)

### 6. Settings Menu Scroll Animation Not Triggered
[App_Settings.ino:21-24](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Settings.ino#L21-L24): When scrolling the settings menu, the `menuScrollAnimY` is **never set**. Compare with Pomodoro (`menuScrollAnimY = encoderDelta * 14`) and Alarm (`menuScrollAnimY = encoderDelta * 14`). The Settings menu just snaps without any scroll animation.

**Fix:** Add `menuScrollAnimY = encoderDelta * 14;` after updating `settingsMenuSelect`.

---

### 7. Tetris Renders Off-Screen (Y Coordinates > 64)
[TimOS.ino:964-980](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L964-L980): The Tetris menu and game-over screens use Y coordinates like `80`, `100` which are **beyond the 64-pixel screen height** in standard landscape mode. Even in portrait mode (rotation 3), the display is only 128px tall, so `y=100` is fine, but `y=80` might clip the `"[Click]"` text. The board itself (`boardPH = 20*5+2 = 102px`) starts at `y=16`, making the total height `118px`, which fits in portrait but is very tight.

**Fix:** Verify all Y coordinates are within bounds for the rotated 64×128 portrait mode.

---

### 8. Battery Percentage Draws Over All Screens Including Tetris
[TimOS.ino:1354](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L1354): `drawBatteryStatus()` is called **before every app's draw function**, including Tetris (which uses rotation 3). The battery icon at coordinates (108, 2) will render in a potentially wrong position in Tetris's rotated coordinate space.

**Fix:** Skip `drawBatteryStatus()` when `currentOSState == OS_APP_TETRIS`.

---

### 9. Alarm Menu Has Only 2 Items But Uses 3-Item Carousel Layout
[TimOS.ino:476-521](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L476-L521): The alarm menu carousel renders `offset -1 to +1` but only has 2 items with a `continue` guard (`if (optIdx < 0 || optIdx >= 2) continue`). This means when `alarmMenuSelect=0`, only 2 of 3 slots render (center + bottom). When `alarmMenuSelect=1`, only 2 slots render (top + center). The top or bottom slot is always empty, making the carousel feel lopsided.

**Fix:** For a 2-item menu, consider using a simpler fixed layout instead of the carousel pattern, or add a 3rd option (e.g., "Snooze Duration").

---

### 10. No Encoder Debouncing
[TimOS.ino:561-573](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L561-L573): The encoder reads raw GPIO state with no software debouncing. On cheap KY-040 encoders, contact bounce can generate phantom `+1` / `-1` pulses during a single detent. The user reported "lag" in menus earlier, which could partly be caused by phantom bounces canceling out intentional movement.

**Fix:** Add a minimum time threshold (e.g., 2-5ms) between accepting encoder transitions:
```cpp
static unsigned long lastEncoderTime = 0;
if (millis() - lastEncoderTime < 3) return 0; // Debounce
lastEncoderTime = millis();
```

---

### 11. `delay()` Calls Inside Sound Functions Block Everything
[TimOS.ino:678-696](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L678-L696): `playSelectTone()`, `playCancelTone()`, and `playFinishedMelody()` use blocking `delay()` calls (60ms, 120ms, and ~1 second respectively). During these delays:
- The encoder is not polled (missed inputs)
- The display is not updated
- The alarm checker doesn't fire

**Risk:** The `playFinishedMelody()` blocks the entire system for **~1 second**. If a user tries to interact during this time, their input is lost.

**Fix (Long-term):** Implement a non-blocking tone queue system using `millis()`. 
**Fix (Quick):** At minimum, remove `delay()` from `playSelectTone` and `playCancelTone` — the `tone()` function is already non-blocking on ESP32.

---

## 🟡 Design & Architecture Improvements

### 12. Monolithic `TimOS.ino` (1380 Lines, Mixed Concerns)
The main file contains: display rendering, input handling, state machine logic, sound functions, battery measurement, LED control, sleep management, AND the entire Pomodoro app logic. The Alarm, Settings, Tetris, and Reader apps are properly split into separate `.ino` files, but Pomodoro is not.

**Fix:** Extract `drawPomoMenu()`, `drawCustomTimeInput()`, `drawPomoCountdown()`, `drawPomoFinished()` and the Pomodoro state machine into `App_Pomodoro.ino`.

---

### 13. `setup()` Lives Inside `App_Tetris.ino` 
[App_Tetris.ino:10-72](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Tetris.ino#L10-L72): The global `setup()` function — responsible for initializing the entire operating system (pins, display, preferences, battery, RTC) — is buried inside the Tetris app file. This is extremely confusing for anyone reading the code.

**Fix:** Move `setup()` to `TimOS.ino` where it logically belongs (at the top, before `loop()`).

---

### 14. Forward Declarations Scattered Across Files
- [App_Alarm.ino:1-2](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Alarm.ino#L1-L2): Forward-declares `drawAlarmApp()` and `drawSettingsApp()` — functions it doesn't even call.
- [App_Tetris.ino:1-8](file:///d:/Workspace/Pomodoro/Pomodomo%20simple%2032c3/TimOS/App_Tetris.ino#L1-L8): Forward-declares every handler function.
- [TimOS.ino:155-181](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L155-L181): Has its own set of forward declarations.

Arduino `.ino` files are concatenated by the IDE, so these scattered declarations work but are confusing.

**Fix:** Consolidate all forward declarations into `TimOS.ino`, remove duplicates from app files.

---

### 15. Reader `initSPIFFS()` Called Every Frame
[App_Reader.ino:217](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Reader.ino#L217) and [App_Reader.ino:426](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Reader.ino#L426): Both `drawReaderApp()` and `handleReaderApp()` call `initSPIFFS()` at the top. While `initSPIFFS()` has a guard (`if (spiffsInitialized) return`), it's still an unnecessary function call overhead on every single frame render and every input event.

**Fix:** Call `initSPIFFS()` once when entering the Reader app (`OS_APP_READER`), not on every draw/input cycle.

---

### 16. Word Cache Only Stores 1 Word
[App_Reader.ino:24-25](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Reader.ino#L24-L25): The reader caches exactly one word (`cachedCurrentWord`). However, the timing engine in `loop()` **also** calls `getCurrentWord(readerWordIndex)` at [TimOS.ino:1108](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L1108) to check punctuation, and `drawReaderApp()` calls it again to render. Because the same word index is requested twice per frame, the cache works. But if any future feature needs the next/previous word (e.g., context preview), it would trigger a full SPIFFS file seek.

**Fix:** Consider caching a small window of 3-5 words around the current index.

---

### 17. `menuScrollAnimY` is a Single Global Shared Across All Menus
The variable `menuScrollAnimY` is used by the App Menu (horizontal carousel), Pomodoro, Alarm, Settings (vertical carousels), and Reader (book selection). If you scroll in one menu and quickly switch to another, the residual animation offset leaks across menus.

**Fix:** Reset `menuScrollAnimY = 0` whenever transitioning between OS states.

---

## 🟢 Minor Polish & UX Improvements

### 18. Battery Redraws Every 2 Seconds Even If Unchanged
[TimOS.ino:1059-1063](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L1059-L1063): The battery check forces `uiChanged = true` every 2 seconds, triggering a full screen redraw even if the battery level hasn't changed. On the launcher screen, this causes a redraw every 1 second (clock) AND every 2 seconds (battery), so battery is redundant.

**Fix:** Only set `uiChanged = true` if the battery percentage actually changed from the last reading.

---

### 19. Pomodoro Timer Drift
[TimOS.ino:1067-1069](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/TimOS.ino#L1067-L1069): The Pomodoro countdown uses `millis()` relative timing (`if (now - lastPomoTick >= 1000)`). Because `loop()` has variable execution time (especially during sound `delay()` calls), the timer will drift slightly over a 25-minute session. A 60ms `playSelectTone()` delay means the timer could lose ~1.5 seconds per minute.

**Fix:** Use RTC-based countdown instead of millis-based: read the actual system time and calculate remaining seconds from the start time.

---

### 20. Tetris Speed Decreases Too Aggressively
[App_Tetris.ino:164](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Tetris.ino#L164): `tetrisSpeed = max(100, 500 - tetrisScore)`. After clearing just 5 single-line clears (50 points), the speed drops to 450ms. After 40 points of quad clears (score ~160), the speed is 340ms. At score 400 (very achievable), the game hits max speed (100ms). The difficulty curve is **way too steep**.

**Fix:** Use a gentler curve: `tetrisSpeed = max(100, 500 - (tetrisScore / 5))`.

---

### 21. Tetris Has No Hard-Drop Feature
The encoder only moves pieces left/right, and click rotates. There's no way to hard-drop a piece (instant drop to the bottom). This makes the game feel sluggish at low speeds.

**Fix:** Map `BUTTON_LONG_PRESS` to hard-drop (move piece down until collision), and use a dedicated "back" gesture (e.g., double-click or Home press).

---

### 22. Reader WPM Not Persisted Across Sessions
`readerWpm` is initialized to 250 at boot and never saved to NVS. If the user sets it to 400 WPM and reboots, it resets to 250.

**Fix:** Save and load `readerWpm` using `preferences.putInt("wpm", readerWpm)` / `preferences.getInt("wpm", 250)`.

---

### 23. Reader Progress Bar Uses Filled Rectangle
[App_Reader.ino:414-416](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Reader.ino#L414-L416): The progress bar draws an outline rect and then a filled rect on top. The fill doesn't account for the 2px border padding, so at 100% progress the fill extends beyond the outline border.

**Fix:** Use `display.fillRect(2, 58, progress - 4, 4, SSD1306_WHITE)` inside the border.

---

### 24. No Visual Feedback During Book Loading
[App_Reader.ino:440-443](file:///d:/Workspace/Pomodoro/Pomodoro%20simple%2032c3/TimOS/App_Reader.ino#L440-L443): The "Loading book..." screen is good, but it only shows for the first load. On subsequent selections of the same book (line 447), the index is **rebuilt silently** with no loading indicator. For a 177KB book (04_Lifecycle_of_Software.txt), this could take 500ms+ of apparent freeze.

**Fix:** Always show the loading indicator when rebuilding the index.

---

## 📊 Summary Table

| Priority | Count | Category |
|:---:|:---:|:---|
| 🔴 Critical | 5 | Crash risks, RAM waste, flash wear |
| 🟠 Significant | 6 | Functional bugs, missing features |
| 🟡 Design | 6 | Architecture & code organization |
| 🟢 Polish | 7 | UX improvements, minor fixes |
| **Total** | **24** | |

---

## 🎯 Recommended Fix Order

1. **Quick Wins (5 min each):** #1 (delete build_reader.py), #2 (remove dead vars), #5 (debounce NVS writes), #6 (settings anim), #17 (reset animY on transition), #22 (persist WPM)
2. **Medium Effort (15 min each):** #8 (skip battery in Tetris), #10 (encoder debounce), #13 (move setup), #15 (init SPIFFS once)
3. **Larger Refactors:** #11 (non-blocking sound), #12 (extract Pomodoro), #14 (consolidate declarations), #19 (RTC-based timer)
