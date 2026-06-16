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

#include "hal/esp32/EspAdcInput.h"
#include "hal/esp32/EspEinkDisplay.h"
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>
#include "ui/SystemUI.h"
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

void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.print("\x1B[2J\x1B[H");
  Serial.println("=== EENK Interactive Fiction Runtime (ESP32-C3) ===");
  Serial.printf("Free heap before init: %u bytes\n", ESP.getFreeHeap());

  display = new DisplayType();
  input = new InputType();
  storage = new EspLittleFSStorage();
  systemUI = new SystemUI(*display);

  bool storyLoaded = false;
  String errorMessage = "";

#ifdef HAS_SD_CARD
  // Try loading story from SD card via flash cache (mmap)
  sdStorage = new EspSdStorage();
  if (sdStorage->begin()) {
    Serial.println("[SD] SD card mounted OK");

    // Discover first .bin file in /eenk
    String sdStoryPathStr = "";
    File root = SD.open("/eenk");
    if (root) {
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
          String name = entry.name();
          if (name.endsWith(".bin")) {
            // ESP32 SD library returns full path in name() or just name? 
            // Better to reconstruct it safely or just use it.
            if (name.startsWith("/")) {
              sdStoryPathStr = name;
            } else {
              sdStoryPathStr = String("/eenk/") + name;
            }
            entry.close();
            break;
          }
        }
        entry.close();
      }
      root.close();
    }

    if (sdStoryPathStr.length() > 0) {
      const char *sdStoryPath = sdStoryPathStr.c_str();
      Serial.printf("[SD] Found story: %s\n", sdStoryPath);

      // Try the flash cache path first (zero-copy mmap for large stories)
      flashCache = new FlashCache();
      const unsigned char *mappedPtr = nullptr;
      std::size_t mappedSize = 0;

      if (flashCache->findPartition()) {
        Serial.println("[FlashCache] app1 partition found, streaming to flash...");
        flashCache->setProgressCallback([](float p, void* ctx) {
          SystemUI* sys = (SystemUI*)ctx;
          sys->showLoading("Loading to Flash...", p);
          Serial.printf("[FlashCache] Loading: %.0f%%\n", p * 100);
        }, systemUI);

        if (flashCache->loadStoryStreaming(*sdStorage, sdStoryPath, &mappedPtr, &mappedSize)) {
          engine = new InkEngine(*display, *input, *storage);
          if (engine->loadStoryFromMemory(mappedPtr, mappedSize)) {
            Serial.printf("[FlashCache] Story mmap'd from flash (%u bytes, zero-copy)\n", (unsigned)mappedSize);
            storyLoaded = true;
          } else {
            errorMessage = "InkCPP failed to parse mmap'd story";
            delete engine;
            engine = nullptr;
          }
        } else {
          errorMessage = "Failed to stream story to flash";
        }
      } else {
        Serial.println("[FlashCache] app1 partition not found, loading to RAM...");
      }

      // Fallback: load directly into RAM (works for small stories <200KB)
      if (!storyLoaded && errorMessage.length() == 0) {
        std::size_t storySize = 0;
        const unsigned char *storyData = sdStorage->readFileBinary(sdStoryPath, &storySize);
        if (storyData && storySize > 0) {
          engine = new InkEngine(*display, *input, *storage);
          if (engine->loadStoryFromMemory(storyData, storySize)) {
            Serial.printf("[SD] Story loaded to RAM (%u bytes)\n", (unsigned)storySize);
            storyLoaded = true;
          } else {
            errorMessage = "InkCPP failed to parse story from RAM";
            delete engine;
            engine = nullptr;
            sdStorage->freeBuffer(storyData);
          }
        } else {
          errorMessage = "Failed to read story file into RAM";
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
    
    while(true) {
#ifdef PLATFORM_ESP32
      ButtonEvent ev = input->pollInput();
      if (ev == ButtonEvent::SLEEP) {
        systemUI->showSleepCover();
        delay(500);
        esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
        esp_deep_sleep_start();
      }
#endif
      delay(100);
    }
  }

  Serial.printf("Free heap after load: %u bytes\n", ESP.getFreeHeap());
  delay(2000); // Give user time to see RAM stats


}

void loop() {
  if (engine && engine->shouldSleep()) {
#ifdef PLATFORM_ESP32
    Serial.println("Power off requested. Entering deep sleep...");
    systemUI->showSleepCover();
    delay(500); // Give e-ink time to finish updating
    esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
#else
    Serial.println("Power off requested (not supported on native).");
    delay(1000);
#endif
  } else if (engine && !engine->isDone()) {
    engine->update();
  } else {
#ifdef PLATFORM_ESP32
    Serial.println("Engine done. Restarting...");
    delay(1000);
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
