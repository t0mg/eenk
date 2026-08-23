// eenk — BootManager
// Persists the boot mode (MENU vs INK_RUNTIME) and the story path to load
// in NVS namespace "boot". On PLATFORM_NATIVE all operations are stubs.
#pragma once
#include <cstddef>
#include <cstdint>

enum class BootMode : uint8_t {
    MENU        = 0,
    INK_RUNTIME = 1,
    BOOK_READER = 2,
};

class BootManager {
public:
    // Initialize NVS (call once at boot before using any other methods).
    static void init();

    // Read the current boot mode from NVS. Defaults to MENU if not set.
    static BootMode getBootMode();

    // Write the boot mode to NVS.
    static void setBootMode(BootMode mode);

    // Store the story path to load in INK_RUNTIME mode.
    static void setStoryPath(const char* path);

    // Retrieve the stored story path. Returns false if not set or buffer too small.
    static bool getStoryPath(char* outPath, size_t maxLen);

    // Reboot the device.
    static void reboot();
    
    // Set OTA_1 as active boot partition and restart
    static void bootUpdater();
};
