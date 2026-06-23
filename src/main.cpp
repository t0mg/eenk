/**
 * EENK - Interactive Fiction Runtime for Xteink X4
 *
 * Milestone 1: SDL2 desktop simulation
 *   - HAL: SDLDisplay (800×480 window), SDLInput (keyboard), SDLStorage (fread)
 *   - Engine: InkEngine state machine driving InkCPP story execution
 *
 * Milestone 2+: ESP32-C3 firmware (Arduino framework)
 *   - Same engine, different HAL implementations
 */

// ════════════════════════════════════════════════════════════════════════════
#ifdef PLATFORM_NATIVE
// ════════════════════════════════════════════════════════════════════════════

#include "engine/InkEngine.h"
#include "hal/sdl/SDLDisplay.h"
#include "hal/sdl/SDLInput.h"
#include "hal/sdl/SDLStorage.h"

#include <SDL.h>
#include <cstdio>

int main(int argc, char *argv[]) {
  const char *storyPath = "stories/the_intercept.bin";
  if (argc > 1)
    storyPath = argv[1];

  printf("=== EENK Interactive Fiction Runtime ===\n");
  printf("Story: %s\n\n", storyPath);

  // Instantiate HAL
  SDLDisplay display;
  SDLInput input(&display); // shares quit-flag
  SDLStorage storage;

  // Check the story file exists before init
  if (!storage.fileExists(storyPath)) {
    fprintf(stderr, "ERROR: Story not found: %s\n", storyPath);
    return 1;
  }

  // Run the engine
  InkEngine engine(display, input, storage);
  if (!engine.loadStory(storyPath)) {
    fprintf(stderr, "ERROR: Failed to load story: %s\n", storyPath);
    return 1;
  }

  while (!engine.isDone() && !display.shouldQuit()) {
    engine.update();
  }

  // Hold the final screen until the user closes the window
  printf("\nStory complete. Close the window or press Escape to exit.\n");
  while (!display.shouldQuit()) {
    ButtonEvent ev = input.pollInput();
    if (ev == ButtonEvent::QUIT || ev == ButtonEvent::BACK)
      break;
    SDL_Delay(16);
  }

  return 0;
}

// ════════════════════════════════════════════════════════════════════════════
#elif defined(PLATFORM_ESP32)
// ════════════════════════════════════════════════════════════════════════════

#include "engine/InkEngine.h"
#include "hal/esp32/EspLittleFSStorage.h"
#ifdef HAS_SD_CARD
#include "hal/esp32/EspSdStorage.h"
#include "hal/esp32/FlashCache.h"
#endif
#include <Arduino.h>
#include <rom/crc.h>

#include "hal/esp32/EspAdcInput.h"
#include "hal/esp32/EspEinkDisplay.h"
#include "os/AppSettings.h"
#include "os/BootManager.h"
#include "ui/BatteryWidget.h"
#include "ui/GameLibrary.h"
#include "ui/SettingsView.h"
#include "ui/SystemUI.h"
#include <BatteryMonitor.h>
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>
#include <esp_sleep.h>

extern const EpdFontFamily OpenSans;

using DisplayType = EspEinkDisplay;
using InputType = EspAdcInput;

DisplayType *display = nullptr;
InputType *input = nullptr;
EspLittleFSStorage *storage = nullptr;
#ifdef HAS_SD_CARD
EspSdStorage *sdStorage = nullptr;
FlashCache *flashCache = nullptr;
#endif
InkEngine *engine = nullptr;
SystemUI *systemUI = nullptr;
BatteryMonitor *batteryMonitor = nullptr;
BatteryWidget *batteryWidget = nullptr;
String saveFilePath = "";
uint32_t currentStoryHash = 0;

void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.print("\x1B[2J\x1B[H");
  Serial.println("=== EENK Interactive Fiction Runtime (ESP32-C3) ===");
  Serial.printf("Free heap before init: %u bytes\n", ESP.getFreeHeap());

  // ── Initialise NVS (must be first — BootManager and AppSettings both use it)
  BootManager::init();

  display = new DisplayType();
  input = new InputType();
  storage = new EspLittleFSStorage();
  systemUI = new SystemUI(*display);

  // ── Battery
  batteryMonitor = new BatteryMonitor(0 /*GPIO0*/);
  batteryWidget = new BatteryWidget(*display->getRenderer(), *batteryMonitor);

  // ── Dual-boot dispatch ─────────────────────────────────────────────────────
  AppSettings settings = AppSettings::load();
  BootMode mode = BootManager::getBootMode();
  Serial.printf("[Boot] mode=%d\n", (int)mode);

  if (mode == BootMode::MENU) {
    // Mount the SD card so GameLibrary can scan for stories.
    sdStorage = new EspSdStorage();
    if (!sdStorage->begin()) {
      Serial.println("[Boot] MENU: SD mount failed — library will show empty.");
    } else {
      Serial.println("[Boot] MENU: SD mounted OK.");
    }

    // Menu / library loop — runs until the user launches a story (which
    // calls BootManager::setBootMode(INK_RUNTIME) and reboots).
    while (true) {
      GameLibrary *library =
          new GameLibrary(*display, *input, *batteryWidget, settings);
      bool goToSettings = library->run();
      delete library;
      if (goToSettings) {
        SettingsView *settingsView =
            new SettingsView(*display, *input, *batteryWidget, settings);
        settingsView->run();
        delete settingsView;
        settings = AppSettings::load(); // reload after save
      }
    }
    // Unreachable: GameLibrary reboots the device when a story is launched.
  }

  // ── INK_RUNTIME — load and run the story selected in the library ──────────
  Serial.println("[Boot] INK_RUNTIME — loading story...");

  bool storyLoaded = false;
  String errorMessage = "";

#ifdef HAS_SD_CARD
  // Try loading story from SD card via flash cache (mmap)
  sdStorage = new EspSdStorage();
  if (sdStorage->begin()) {
    Serial.println("[SD] SD card mounted OK");

    // Discover first .bin file in /eenk
    const char *sdStoryPath = nullptr;
    File eenkDir = SD.open("/eenk");
    if (!eenkDir || !eenkDir.isDirectory()) {
      errorMessage = "/eenk/ directory not found on SD card";
    } else {
      // Ensure saves directory exists
      if (!SD.exists("/.eenk_saves")) {
        SD.mkdir("/.eenk_saves");
      }

      File file = eenkDir.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String filename = file.name();
          if (filename.endsWith(".bin")) {
            // We found a story!
            sdStoryPath =
                new char[filename.length() + 7]; // "/eenk/" + filename
            sprintf((char *)sdStoryPath, "/eenk/%s", filename.c_str());

            saveFilePath = String("/.eenk_saves/") + filename + ".save";
            break;
          }
        }
        file = eenkDir.openNextFile();
      }
      eenkDir.close();
    }

    if (sdStoryPath != nullptr) {
      Serial.printf("[SD] Found story: %s\n", sdStoryPath);

      File storyFile = SD.open(sdStoryPath, FILE_READ);
      size_t storySize = 0;
      if (storyFile) {
        storySize = storyFile.size();
        storyFile.close();
      }

      if (storySize > 8388608) { // 8MB ESP32 MMU hardware limit
        errorMessage = "Story is too large! Max supported size is 8.0MB.";
      } else if (storySize == 0) {
        errorMessage = "Failed to read story file or file is empty.";
      } else {
        // Try the flash cache path first (zero-copy mmap for large stories)
        flashCache = new FlashCache();
        const unsigned char *mappedPtr = nullptr;
        std::size_t mappedSize = 0;

        if (flashCache->findPartition()) {
          Serial.println(
              "[FlashCache] ink_cache partition found, streaming to flash...");
          flashCache->setProgressCallback(
              [](float p, void *ctx) {
                static int lastPercent = -1;
                int currentPercent = (int)(p * 100);
                if (currentPercent != lastPercent) {
                  lastPercent = currentPercent;
                  SystemUI *sys = (SystemUI *)ctx;
                  sys->showLoading("Loading story...", p);
                  Serial.printf("[FlashCache] Loading: %d%%\n", currentPercent);
                }
              },
              systemUI);

          if (flashCache->loadStoryStreaming(*sdStorage, sdStoryPath,
                                             &mappedPtr, &mappedSize)) {
            // Require EENK metadata header
            if (mappedSize >= 128 && memcmp(mappedPtr, "EENK", 4) == 0) {
              mappedPtr += 128;
              mappedSize -= 128;

              engine = new InkEngine(*display, *input, *storage);
              engine->applySettings(AppSettings::load());
              storyLoaded = engine->loadStoryFromMemory(mappedPtr, mappedSize);
              currentStoryHash = flashCache->getHash();
            } else {
              errorMessage =
                  "Story could not be read (missing or invalid metadata).";
              storyLoaded = false;
            }

            if (storyLoaded && SD.exists(saveFilePath)) {
              File saveFile = SD.open(saveFilePath, FILE_READ);
              if (saveFile) {
                uint32_t magic = 0;
                if (saveFile.read((uint8_t *)&magic, 4) == 4 &&
                    magic == 0x314B4E45) { // "ENK1"
                  uint32_t savedHash = 0;
                  if (saveFile.read((uint8_t *)&savedHash, 4) == 4 &&
                      savedHash == currentStoryHash) {
                    uint32_t snapSize = 0;
                    if (saveFile.read((uint8_t *)&snapSize, 4) == 4) {
                      // Cap size to reasonable limit (e.g. 1MB) to prevent bad
                      // allocs on corrupt files
                      if (snapSize < 1024 * 1024) {
                        unsigned char *buf =
                            new (std::nothrow) unsigned char[snapSize];
                        if (buf) {
                          saveFile.read(buf, snapSize);
                          if (engine->loadSnapshot(buf, snapSize)) {
                            Serial.println("Save loaded successfully!");

                            // Read history
                            uint16_t historySize = 0;
                            if (saveFile.read((uint8_t *)&historySize, 2) ==
                                2) {
                              std::deque<InkEngine::WrappedLine> history;
                              for (uint16_t i = 0; i < historySize; i++) {
                                uint16_t lineLen;
                                if (saveFile.read((uint8_t *)&lineLen, 2) != 2)
                                  break;
                                char *lineBuf =
                                    new (std::nothrow) char[lineLen + 1];
                                if (lineBuf) {
                                  saveFile.read((uint8_t *)lineBuf, lineLen);
                                  lineBuf[lineLen] = '\0';
                                  uint8_t isOld;
                                  saveFile.read(&isOld, 1);
                                  history.push_back(
                                      {std::string(lineBuf), isOld > 0});
                                  delete[] lineBuf;
                                } else {
                                  break;
                                }
                              }
                              engine->setHistory(history);
                            }
                          } else {
                            Serial.println("Failed to load save file.");
                            errorMessage = "Failed to load save file.";
                            storyLoaded = false;
                          }
                          delete[] buf;
                        } else {
                          Serial.println("Out of memory reading save.");
                          errorMessage = "Out of memory reading save.";
                          storyLoaded = false;
                        }
                      } else {
                        Serial.println("Save file snapshot too large.");
                        errorMessage = "Save file corrupt (size).";
                        storyLoaded = false;
                      }
                    }
                  } else {
                    Serial.println("Save file is for an older version of the "
                                   "story. Starting fresh.");
                  }
                } else {
                  Serial.println("Incompatible save file. Starting fresh.");
                  // It's an old save file. Just ignore it and start fresh.
                }
                saveFile.close();
              }
            }
          } else {
            errorMessage = "Failed to stream story to flash";
          }
        } else {
          Serial.println("[FlashCache] ink_cache partition not found!");
          errorMessage = "ink_cache partition missing! Update your partitions.";
        }
      }
    } else {
      errorMessage = "No .bin stories found in /eenk/ folder";
    }
  } else {
    errorMessage = "No SD card detected or mount failed";
  }
#endif

  if (!storyLoaded) {
    if (errorMessage.length() == 0) {
      errorMessage = "No story available.";
    }
    Serial.printf("FATAL: %s\n", errorMessage.c_str());

    systemUI->showError("EENK SYSTEM ERROR", errorMessage.c_str());

    while (true) {
#ifdef PLATFORM_ESP32
      ButtonEvent ev = input->pollInput();
      if (ev == ButtonEvent::SLEEP) {
        systemUI->showSleepCover();
        delay(500);
        esp_deep_sleep_enable_gpio_wakeup(
            1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
        esp_deep_sleep_start();
      }
#endif
      delay(100);
    }
  }

  Serial.printf("Free heap after load: %u bytes\n", ESP.getFreeHeap());
  delay(2000); // Give user time to see RAM stats
}

void saveProgress() {
  if (!engine) return;
#ifdef PLATFORM_ESP32
  systemUI->showLoading("Saving Progress...", 1.0f);
  delay(100);

  size_t snapLen = 0;
  const unsigned char *snapData = engine->createSnapshot(&snapLen);
  if (snapData) {
    File f = SD.open(saveFilePath, FILE_WRITE);
    if (f) {
      uint32_t magic = 0x314B4E45; // "ENK1"
      f.write((const uint8_t *)&magic, 4);

      f.write((const uint8_t *)&currentStoryHash, 4);

      uint32_t snapSize32 = snapLen;
      f.write((const uint8_t *)&snapSize32, 4);
      f.write(snapData, snapLen);

      // Write history
      const auto &history = engine->getHistory();
      uint16_t historySize = history.size();
      f.write((const uint8_t *)&historySize, 2);
      for (const auto &line : history) {
        uint16_t len = line.text.length();
        f.write((const uint8_t *)&len, 2);
        f.write((const uint8_t *)line.text.c_str(), len);
        uint8_t isOld = line.isOld ? 1 : 0;
        f.write(&isOld, 1);
      }

      f.close();
      Serial.println("Game saved successfully!");
    } else {
      systemUI->showError("EENK SYSTEM ERROR",
                          "Failed to write save file to SD.");
      while (true) {
        if (input->pollInput() == ButtonEvent::SLEEP)
          break;
        delay(10);
      }
    }
  } else {
    systemUI->showError("EENK SYSTEM ERROR",
                        "Failed to create runtime snapshot.");
    while (true) {
      if (input->pollInput() == ButtonEvent::SLEEP)
        break;
      delay(10);
    }
  }
#endif
}

void loop() {
  if (engine && engine->shouldSleep()) {
#ifdef PLATFORM_ESP32
    saveProgress();

    Serial.println("Power off requested. Entering deep sleep...");
    systemUI->showSleepCover();
    delay(500); // Give e-ink time to finish updating
    esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN,
                                      ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
#else
    Serial.println("Power off requested (not supported on native).");
    delay(1000);
#endif
  } else if (engine && !engine->isDone()) {
    engine->update();
  } else {
#ifdef PLATFORM_ESP32
    // Story finished or user pressed BACK — return to the library.
    Serial.println("Engine done. Returning to MENU...");
    saveProgress();
    if (systemUI) {
      systemUI->showLoading("Loading...", 0.0f);
    }
    BootManager::setBootMode(BootMode::MENU);
    delay(500);
    ESP.restart();
#else
    delay(100);
#endif
  }
}

// ════════════════════════════════════════════════════════════════════════════
#else
#error "No platform defined. Define PLATFORM_NATIVE or PLATFORM_ESP32."
#endif
