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

#include "hal/sdl/SDLDisplay.h"
#include "hal/sdl/SDLInput.h"
#include "hal/sdl/SDLStorage.h"
#include "engine/InkEngine.h"

#include <SDL.h>
#include <cstdio>

int main(int argc, char* argv[])
{
    const char* storyPath = "stories/hello_world.bin";
    if (argc > 1) storyPath = argv[1];

    printf("=== EENK Interactive Fiction Runtime ===\n");
    printf("Story: %s\n\n", storyPath);

    // Instantiate HAL
    SDLDisplay display;
    SDLInput   input(&display);  // shares quit-flag
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
        if (ev == ButtonEvent::QUIT || ev == ButtonEvent::BACK) break;
        SDL_Delay(16);
    }

    return 0;
}

// ════════════════════════════════════════════════════════════════════════════
#elif defined(PLATFORM_ESP32)
// ════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "hal/esp32/EspLittleFSStorage.h"
#include "engine/InkEngine.h"

#ifdef SERIAL_DEBUG
// ── Debug mode: render to serial terminal, no e-ink hardware required ───────
#include "hal/esp32/EspSerialDisplay.h"
#include "hal/esp32/EspSerialInput.h"
using DisplayType = EspSerialDisplay;
using InputType   = EspSerialInput;
#else
// ── Hardware mode: e-ink display + ADC D-Pad (Xteink X4) ─────────────────
#include "hal/esp32/EspEinkDisplay.h"
#include "hal/esp32/EspAdcInput.h"
using DisplayType = EspEinkDisplay;
using InputType   = EspAdcInput;
#endif

DisplayType*         display = nullptr;
InputType*           input   = nullptr;
EspLittleFSStorage*  storage = nullptr;
InkEngine*           engine  = nullptr;

void setup()
{
    Serial.begin(115200);
    
#ifdef SERIAL_DEBUG
    // Wait for the serial monitor to connect before booting the game
    while (!Serial) {
        delay(10);
    }
    // VT100 clear is handled by EspSerialDisplay constructor
    Serial.println("=== EENK Interactive Fiction Runtime (ESP32-C3 / Serial Debug) ===");
#else
    delay(1000);
    Serial.print("\x1B[2J\x1B[H");
    Serial.println("=== EENK Interactive Fiction Runtime (ESP32-C3) ===");
#endif
    Serial.printf("Free heap before init: %u bytes\n", ESP.getFreeHeap());

    display = new DisplayType();
    input   = new InputType();
    storage = new EspLittleFSStorage();

    engine = new InkEngine(*display, *input, *storage);

    const char* storyPath = "/the_intercept.bin";
    if (!storage->fileExists(storyPath)) {
        Serial.printf("ERROR: Story not found in Embedded Flash: %s\n", storyPath);
        return;
    }

    if (!engine->loadStory(storyPath)) {
        Serial.println("ERROR: Failed to load story!");
        return;
    }

    Serial.printf("Free heap after load: %u bytes\n", ESP.getFreeHeap());
    delay(2000); // Give user time to see RAM stats

#ifdef SERIAL_DEBUG
    Serial.println("Controls: W/S = Up/Down, Enter = Confirm, B = Back, Q = Quit");
    Serial.println("────────────────────────────────────────");
#endif
}

void loop()
{
    if (engine && !engine->isDone()) {
        engine->update();
    } else {
        delay(100);
    }
}

// ════════════════════════════════════════════════════════════════════════════
#else
#error "No platform defined. Define PLATFORM_NATIVE or PLATFORM_ESP32."
#endif
