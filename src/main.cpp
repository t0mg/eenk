/**
 * eenk - Interactive Fiction Runtime for Xteink X4
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

#include "book/BookEngine.h"
#include "engine/InkEngine.h"
#include "hal/sdl/SDLDisplay.h"
#include "hal/sdl/SDLInput.h"
#include "hal/sdl/SDLStorage.h"

#include <SDL.h>
#include <cstdio>

#ifndef PIO_UNIT_TESTING
int main(int argc, char *argv[]) {
  const char *storyPath = "stories/the_intercept.bin";
  int winW = SDLDisplay::WIN_W;
  int winH = SDLDisplay::WIN_H;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--x3") == 0) {
      winW = SDLDisplay::X3_WIN_W;
      winH = SDLDisplay::X3_WIN_H;
    } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
      if (strcasecmp(argv[i + 1], "x3") == 0) {
        winW = SDLDisplay::X3_WIN_W;
        winH = SDLDisplay::X3_WIN_H;
      }
      i++;
    } else if (argv[i][0] != '-') {
      storyPath = argv[i];
    }
  }

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  printf("=== eenk Interactive Fiction Runtime ===\n");
  printf("Story: %s\n", storyPath);
  printf("Display: %d×%d\n\n", winW, winH);

  // Instantiate HAL
  SDLDisplay display(winW, winH);
  SDLInput input(&display); // shares quit-flag
  SDLStorage storage;

  // Check the story file exists before init
  if (!storage.fileExists(storyPath)) {
    fprintf(stderr, "ERROR: File not found: %s\n", storyPath);
    return 1;
  }

  bool isEpub = (strlen(storyPath) > 5 &&
                 strcasecmp(storyPath + strlen(storyPath) - 5, ".epub") == 0);
  if (isEpub) {
    BookEngine bookEngine(display, input);
    AppSettings settings = AppSettings::load();
    bookEngine.applySettings(settings);
    input.setAutoSleepTimeout(settings.sleepTimeoutSec);
    if (!bookEngine.loadBook(storyPath)) {
      fprintf(stderr, "ERROR: Failed to load book: %s\n", storyPath);
      return 1;
    }
    while (!bookEngine.isDone() && !display.shouldQuit()) {
      bookEngine.update();
    }
    return 0;
  }

  // Run the engine
  InkEngine engine(display, input, storage);
  AppSettings settings = AppSettings::load();
  engine.applySettings(settings);
  input.setAutoSleepTimeout(settings.sleepTimeoutSec);
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
#endif

// ════════════════════════════════════════════════════════════════════════════
#elif defined(PLATFORM_ESP32)
// ════════════════════════════════════════════════════════════════════════════

#include "book/BookEngine.h"
#include "engine/InkEngine.h"

#include "HalInit.h"
#include "HalTypes.h"

#if HAS_FLASH_CACHE
#include "hal/esp32/common/FlashCache.h"
#endif
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <rom/crc.h>

#include "hal/IFrontlight.h"
#include "os/AppSettings.h"
#include "os/BootManager.h"
#include "serial/SerialFileServer.h"
#include "ui/BatteryWidget.h"
#include "ui/Library.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/StoryMetadata.h"
#include "ui/SystemUI.h"
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>
#include <esp_sleep.h>

extern const EpdFontFamily OpenSans;

DisplayType *display = nullptr;
InputType *input = nullptr;
IStorage *storage = nullptr;
IFrontlight *frontlight = nullptr;
#ifdef HAS_SD_CARD
StorageType *sdStorage = nullptr;
#if HAS_FLASH_CACHE
FlashCache *flashCache = nullptr;
#endif
#endif
InkEngine *engine = nullptr;
BookEngine *bookEngine = nullptr;
SystemUI *systemUI = nullptr;
BatteryMonitor *batteryMonitor = nullptr;
BatteryWidget *batteryWidget = nullptr;
String saveFilePath = "";
uint32_t currentStoryHash = 0;

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < 2000)) {
    delay(10);
  }
#else
  delay(1000);
#endif
  Serial.print("\x1B[2J\x1B[H");
  Serial.println("=== eenk Interactive Fiction Runtime ===");
  Serial.printf("Free heap before init: %u bytes\n", (unsigned int)ESP.getFreeHeap());

  // ── Initialise NVS (must be first — BootManager and AppSettings both use it)
  BootManager::init();

  // ── Early check for updater mode ──
  HalInit::earlyBootCheck();

  display = new DisplayType();
  input = new InputType();
  frontlight = HalInit::createFrontlight();

  // Storage is assigned later in the STORY boot branch.
  // storage = new EspLittleFSStorage();
  systemUI = new SystemUI(*display);

  // ── Battery
  batteryMonitor = HalInit::createBatteryMonitor();
  batteryWidget = new BatteryWidget(*display->getRenderer(), *batteryMonitor);

  // ── Dual-boot dispatch ─────────────────────────────────────────────────────
  AppSettings settings = AppSettings::load();
  input->setAutoSleepTimeout(settings.sleepTimeoutSec);
  if (frontlight) {
    if (settings.frontLightEnabled) {
      frontlight->setBrightness(settings.frontLightBrightness);
      frontlight->setColorTemperature(settings.frontLightTemperature);
    } else {
      frontlight->setBrightness(0);
    }
  }
  BootMode mode = BootManager::getBootMode();
  Serial.printf("[Boot] mode=%d\n", (int)mode);

  if (mode == BootMode::MENU) {
    // Mount the SD card so Library can scan for stories.
    sdStorage = new StorageType();
    if (!sdStorage->begin()) {
      Serial.println("[Boot] MENU: SD mount failed — library will show empty.");
    } else {
      Serial.println("[Boot] MENU: SD mounted OK.");
    }

    // Menu / library loop — runs until the user launches a story (which
    // calls BootManager::setBootMode(INK_RUNTIME) and reboots).
    bool needsFullRefresh = true;

    SerialFileServer serialServer;
    serialServer.onConnect = [&]() {
      if (input)
        input->setAutoSleepTimeout(0); // Disable autosleep during USB sync
      systemUI->showMessage("USB Connected",
                            "Please do not unplug or power off.");
    };

#ifdef PLATFORM_ESP32
    const int powerPin = BoardConfig::ACTIVE.input.power;
    const int powerActiveLevel = BoardConfig::ACTIVE.input.powerActiveHigh ? HIGH : LOW;
    if (powerPin >= 0) {
      while (digitalRead(powerPin) == powerActiveLevel) {
        delay(10);
      }
    }
#endif
    if (input) input->resetActivityTimer();

    while (true) {
        Library *library =
            new Library(*display, *input, *batteryWidget, frontlight, settings);
      library->setNeedsFullRefresh(needsFullRefresh);
      library->setBackgroundTask(
          [&serialServer]() { return serialServer.poll(); });
      bool goToSettings = library->run();
      delete library;
      if (goToSettings) {
        SettingsView *settingsView =
            new SettingsView(*display, *input, *batteryWidget, frontlight, settings);
        settingsView->run();
        delete settingsView;
        settings = AppSettings::load(); // reload after save
        input->setAutoSleepTimeout(
            settings.sleepTimeoutSec); // update timeout in case it changed
        needsFullRefresh = false; // Came from settings, avoid slow full refresh
      }
    }
    // Unreachable: Library reboots the device when a story is launched.
  }

  // ── BOOK_READER — load and run the EPUB selected in the library ───────────
  if (mode == BootMode::BOOK_READER) {
    Serial.println("[Boot] BOOK_READER — loading book...");
#ifdef HAS_SD_CARD
    sdStorage = new StorageType();
    if (!sdStorage->begin()) {
      Serial.println("[SD] SD mount failed for book reader.");
      systemUI->showMessage("SD ERROR", "Failed to mount SD card.\n\nPress BACK to return to menu.");
      while (true) {
        ButtonEvent ev = input->pollInput();
        if (ev == ButtonEvent::BACK || ev == ButtonEvent::CONFIRM || ev == ButtonEvent::QUIT) {
          BootManager::setBootMode(BootMode::MENU);
          delay(500);
          BootManager::reboot();
        }
        delay(10);
      }
    }

    char bookPath[128] = {0};
    if (!BootManager::getStoryPath(bookPath, sizeof(bookPath)) || bookPath[0] == '\0') {
      systemUI->showMessage("BOOK ERROR", "No book selected.\n\nPress BACK to return to menu.");
      while (true) {
        ButtonEvent ev = input->pollInput();
        if (ev == ButtonEvent::BACK || ev == ButtonEvent::CONFIRM || ev == ButtonEvent::QUIT) {
          BootManager::setBootMode(BootMode::MENU);
          delay(500);
          BootManager::reboot();
        }
        delay(10);
      }
    }

    bookEngine = new BookEngine(*display, *input);
    bookEngine->setFrontlight(frontlight);
    bookEngine->setBatteryWidget(batteryWidget);
    bookEngine->applySettings(settings);

    if (!bookEngine->loadBook(bookPath)) {
      systemUI->showMessage("BOOK ERROR", "Failed to open book.\n\nPress BACK to return to menu.");
      while (true) {
        ButtonEvent ev = input->pollInput();
        if (ev == ButtonEvent::BACK || ev == ButtonEvent::CONFIRM || ev == ButtonEvent::QUIT) {
          BootManager::setBootMode(BootMode::MENU);
          delay(500);
          BootManager::reboot();
        }
        delay(10);
      }
    }

#ifdef PLATFORM_ESP32
    const int powerPin = BoardConfig::ACTIVE.input.power;
    const int powerActiveLevel = BoardConfig::ACTIVE.input.powerActiveHigh ? HIGH : LOW;
    if (powerPin >= 0) {
      while (digitalRead(powerPin) == powerActiveLevel) {
        delay(10);
      }
    }
#endif
    if (input) input->resetActivityTimer();

    return; // setup() done, loop() will run bookEngine
#else
    systemUI->showMessage("SD ERROR", "SD card not supported on this build.");
    while (true) { delay(100); }
#endif
  }

  // ── INK_RUNTIME — load and run the story selected in the library ──────────
  Serial.println("[Boot] INK_RUNTIME — loading story...");

  bool storyLoaded = false;
  String errorMessage = "";

#ifdef HAS_SD_CARD
  // Try loading story from SD card via flash cache (mmap)
  sdStorage = new StorageType();
  if (sdStorage->begin()) {
    Serial.println("[SD] SD card mounted OK");

    const char *sdStoryPath = nullptr;

    // Ensure saves directory exists
    if (!SD_FS.exists("/.eenk_saves")) {
      SD_FS.mkdir("/.eenk_saves");
    }

    char savedPath[128] = {0};
    if (BootManager::getStoryPath(savedPath, sizeof(savedPath)) &&
        savedPath[0] != '\0') {
      if (SD_FS.exists(savedPath)) {
        size_t len = strlen(savedPath);
        char *pathCopy = new char[len + 1];
        strcpy(pathCopy, savedPath);
        sdStoryPath = pathCopy;

        char saveBuf[256] = {};
        StoryMetadata::getSavePath(savedPath, saveBuf, sizeof(saveBuf));
        saveFilePath = String(saveBuf);
      } else {
        errorMessage = String("Selected story not found: ") + savedPath;
      }
    } else {
      errorMessage = "No story selected. Please return to the menu.";
    }

    if (sdStoryPath != nullptr) {
      Serial.printf("[SD] Found story: %s\n", sdStoryPath);

      File storyFile = SD_FS.open(sdStoryPath, FILE_READ);
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
#if HAS_FLASH_CACHE
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
            // Require eenk metadata header
            if (mappedSize >= 128 && memcmp(mappedPtr, "eenk", 4) == 0) {
              storage = sdStorage;
              engine = new InkEngine(*display, *input, *storage);
              engine->setFrontlight(frontlight);
              engine->setBatteryWidget(batteryWidget);
              engine->applySettings(AppSettings::load());
              storyLoaded =
                  engine->loadStory(mappedPtr, mappedSize, sdStoryPath);
              currentStoryHash = flashCache->getHash();
            } else {
              errorMessage =
                  "Story could not be read (missing or invalid metadata).";
              storyLoaded = false;
            }
          } else {
            errorMessage = "Failed to stream story to flash";
          }
        } else {
          Serial.println("[FlashCache] ink_cache partition not found!");
          errorMessage = "ink_cache partition missing! Update your partitions.";
        }
#else
        std::size_t mappedSize = 0;
        const unsigned char *mappedPtr =
            sdStorage->readFileBinary(sdStoryPath, &mappedSize);
        if (mappedPtr != nullptr && mappedSize >= 128 &&
            memcmp(mappedPtr, "eenk", 4) == 0) {
          storage = sdStorage;
          engine = new InkEngine(*display, *input, *storage);
          engine->setFrontlight(frontlight);
          engine->setBatteryWidget(batteryWidget);
          engine->applySettings(AppSettings::load());
          storyLoaded = engine->loadStory(mappedPtr, mappedSize, sdStoryPath);
          currentStoryHash = crc32_le(0, mappedPtr, mappedSize);
          // DO NOT free mappedPtr here! InkEngine uses it in-place and relies
          // on it persisting.
        } else {
          errorMessage = "Story could not be read or invalid metadata.";
          storyLoaded = false;
        }
#endif

        if (storyLoaded) {
          Serial.println("Story and save loaded successfully!");
        }
      }
    } else {
      errorMessage = "No .bin stories found in /stories/ folder";
    }
  } else {
    errorMessage = "No SD card detected or mount failed";
  }
#endif

  if (!storyLoaded) {
    if (errorMessage.length() == 0) {
      errorMessage = "No story available.";
    }
    errorMessage += "\n\nPress any button to reboot to the menu.";
    Serial.printf("FATAL: %s\n", errorMessage.c_str());

    systemUI->showMessage("eenk SYSTEM ERROR", errorMessage.c_str());

    while (true) {
#ifdef PLATFORM_ESP32
      ButtonEvent ev = input->pollInput();
      if (ev == ButtonEvent::SLEEP) {
        systemUI->showSleepCover();
        delay(500);
        HalInit::prepareForSleep();
        esp_deep_sleep_start();
      } else if (ev == ButtonEvent::CONFIRM || ev == ButtonEvent::BACK ||
                 ev == ButtonEvent::QUIT || ev == ButtonEvent::LEFT ||
                 ev == ButtonEvent::RIGHT || ev == ButtonEvent::UP ||
                 ev == ButtonEvent::DOWN) {
        Serial.println("Rebooting to MENU requested by user...");
        BootManager::setBootMode(BootMode::MENU);
        delay(500);
        BootManager::reboot();
      }
#endif
      delay(100);
    }
  }

  Serial.printf("Free heap after load: %u bytes\n", (unsigned int)ESP.getFreeHeap());
#ifdef PLATFORM_ESP32
  const int powerPin = BoardConfig::ACTIVE.input.power;
  const int powerActiveLevel = BoardConfig::ACTIVE.input.powerActiveHigh ? HIGH : LOW;
  if (powerPin >= 0) {
    while (digitalRead(powerPin) == powerActiveLevel) {
      delay(10);
    }
  }
#endif
  if (input) input->resetActivityTimer();
  delay(2000); // Give user time to see RAM stats
  if (input) input->resetActivityTimer();
}

void saveProgress() {
  if (!engine || !storage)
    return;
#ifdef PLATFORM_ESP32
  systemUI->showLoading("Saving Progress...", 1.0f);
  delay(100);

  size_t snapLen = 0;
  const unsigned char *snapData = engine->createSnapshot(&snapLen);
  if (snapData && snapLen > 0) {
    engine->getSaveManager().saveMainProgress(snapData, snapLen,
                                             engine->getHistory());
    if (engine->getSaveManager().writeSaveFile(*storage)) {
      Serial.println("Game saved successfully!");
    } else {
      Serial.println("Failed to write save file to SD.");
    }
  }
#endif
}

void loop() {
  SystemUI::checkBatteryAndShutdown(*batteryWidget, *display);

  if (bookEngine) {
    if (bookEngine->shouldSleep()) {
#ifdef PLATFORM_ESP32
      BootManager::setBootMode(BootMode::BOOK_READER);
      Serial.println("Power off requested. Entering deep sleep...");
      systemUI->showSleepCover("Sleeping...", "Book Reader");
      delay(500);
      HalInit::prepareForSleep();
      esp_deep_sleep_start();
#endif
    } else if (!bookEngine->isDone()) {
      bookEngine->update();
    } else {
#ifdef PLATFORM_ESP32
      Serial.println("Book done. Returning to MENU...");
      if (systemUI) {
        systemUI->showLoading("Loading...", 1.0f);
      }
      BootManager::setBootMode(BootMode::MENU);
      delay(500);
      ESP.restart();
#endif
    }
    return;
  }

  if (engine && engine->shouldSleep()) {
#ifdef PLATFORM_ESP32
    saveProgress();
    BootManager::setBootMode(BootMode::INK_RUNTIME);

    Serial.println("Power off requested. Entering deep sleep...");

    char titleBuf[128] = {0};
    char savedPath[128] = {0};
    if (BootManager::getStoryPath(savedPath, sizeof(savedPath)) &&
        savedPath[0] != '\0') {
      StoryMetadata meta;
      if (StoryMetadata::readFromSD(savedPath, &meta) &&
          meta.title[0] != '\0') {
        snprintf(titleBuf, sizeof(titleBuf), "%s", meta.title);
      }
    }

    if (titleBuf[0] != '\0') {
      systemUI->showSleepCover("Sleeping...", titleBuf);
    } else {
      systemUI->showSleepCover();
    }

    delay(500); // Give e-ink time to finish updating
    HalInit::prepareForSleep();
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
      systemUI->showLoading("Loading...", 1.0f);
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
