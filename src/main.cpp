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
#endif
#include <Arduino.h>

#include "hal/esp32/EspAdcInput.h"
#include "hal/esp32/EspEinkDisplay.h"
using DisplayType = EspEinkDisplay;
using InputType = EspAdcInput;

DisplayType *display = nullptr;
InputType *input = nullptr;
EspLittleFSStorage *storage = nullptr;
#ifdef HAS_SD_CARD
EspSdStorage *sdStorage = nullptr;
#endif
InkEngine *engine = nullptr;

void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.print("\x1B[2J\x1B[H");
  Serial.println("=== EENK Interactive Fiction Runtime (ESP32-C3) ===");
  Serial.printf("Free heap before init: %u bytes\n", ESP.getFreeHeap());

  display = new DisplayType();
  input = new InputType();
  storage = new EspLittleFSStorage();

  bool storyLoaded = false;

#ifdef HAS_SD_CARD
  // Try loading story from SD card first
  sdStorage = new EspSdStorage();
  if (sdStorage->begin()) {
    Serial.println("[SD] SD card mounted OK");

    const char *sdStoryPath = "/eenk/the_intercept.bin";
    if (sdStorage->fileExists(sdStoryPath)) {
      Serial.printf("[SD] Found story: %s\n", sdStoryPath);

      std::size_t storySize = 0;
      const unsigned char *storyData = sdStorage->readFileBinary(sdStoryPath, &storySize);
      if (storyData && storySize > 0) {
        engine = new InkEngine(*display, *input, *storage);
        if (engine->loadStoryFromMemory(storyData, storySize)) {
          Serial.printf("[SD] Story loaded from SD card (%u bytes)\n", (unsigned)storySize);
          storyLoaded = true;
        } else {
          Serial.println("[SD] ERROR: InkCPP failed to parse story from SD");
          delete engine;
          engine = nullptr;
        }
        // Note: storyData must remain valid for the engine's lifetime.
        // InkCPP reads from the pointer directly — do NOT free it.
      } else {
        Serial.println("[SD] ERROR: Failed to read story file");
      }
    } else {
      Serial.printf("[SD] Story not found on SD: %s\n", sdStoryPath);
    }
  } else {
    Serial.println("[SD] No SD card detected, falling back to embedded story");
  }
#endif

  // Fall back to embedded LittleFS story if SD didn't work
  if (!storyLoaded) {
    const char *storyPath = "/the_intercept.bin";
    if (storage->fileExists(storyPath)) {
      engine = new InkEngine(*display, *input, *storage);
      if (engine->loadStory(storyPath)) {
        Serial.printf("[LittleFS] Story loaded from embedded flash\n");
        storyLoaded = true;
      } else {
        Serial.println("ERROR: Failed to load embedded story!");
      }
    } else {
      Serial.printf("ERROR: No story found (no SD card file, no embedded file)\n");
    }
  }

  if (!storyLoaded) {
    Serial.println("FATAL: No story available. Halting.");
    while(true) delay(100);
  }

  Serial.printf("Free heap after load: %u bytes\n", ESP.getFreeHeap());
  delay(2000); // Give user time to see RAM stats


}

void loop() {
  if (engine && !engine->isDone()) {
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
