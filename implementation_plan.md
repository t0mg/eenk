# EENK — Interactive Fiction Runtime for Xteink X4

## Overview

Transform the Xteink X4 e-reader into an interactive fiction console by embedding the **InkCPP** runtime into a fork of the **Papyrix Reader** firmware. The ultimate architecture (Option 3 — SPI Flash Memory Mapping) delivers zero-SRAM-impact execution of multi-megabyte ink games. We reach that goal incrementally through seven milestones, starting with a desktop proof-of-concept and ending with a polished dual-boot OS with menu, settings, and save/resume support.

---

## Emulation & Simulation Strategy

Since we don't have the physical device yet, we need a layered simulation approach. Rather than trying to perfectly emulate the Xteink X4's custom hardware (SSD1677 e-ink, resistor-ladder buttons) — which would require building custom QEMU peripherals or Wokwi custom chips — we adopt a **three-tier development strategy**:

### Tier 1 — Native Desktop (Primary Development Environment)

| Aspect | Detail |
|:---|:---|
| **Platform** | PlatformIO `[env:native]` — compiles with host GCC/MSVC |
| **Purpose** | All business logic, InkCPP integration, state machine, and text rendering pipeline |
| **Display** | SDL2 window (480×800 monochrome) simulating the e-ink framebuffer via Papyrix's GfxRenderer and EInkDisplay in mock mode |
| **Input** | Keyboard mapping: Arrow keys → UP/DOWN/LEFT/RIGHT, Enter → CONFIRM, Escape → BACK |
| **Storage** | Local filesystem standing in for SD card; binary file I/O |

> [!TIP]
> This tier gives us **instant iteration** — sub-second build times, full debugger support, and visual output without needing any hardware or cloud services.

### Tier 2 — ESP32-C3 Serial Debug (`esp32c3_serial` environment)

| Aspect | Detail |
|:---|:---|
| **Platform** | PlatformIO `[env:esp32c3_serial]` — targets ESP32-C3 dev boards without e-ink hardware |
| **Purpose** | Validate firmware compiles and runs on real RISC-V target; test memory budgets |
| **Display** | `EspSerialDisplay` — VT100/ANSI escape codes rendering the game in a serial terminal |
| **Input** | `EspSerialInput` — polling `Serial.read()` for navigation keys (W/S/Enter/B/Q) |
| **Storage** | Story binary embedded in flash via `board_build.embed_txtfiles`, accessed through linker symbols |

> [!NOTE]
> This tier validates that firmware fits in the ESP32-C3 memory budget and that FreeRTOS scheduling behaves correctly. The `lib_ignore = EInkDisplay` setting avoids pulling in the SSD1677 driver.

### Tier 3 — Physical Xteink X4 (`esp32c3` environment)

| Aspect | Detail |
|:---|:---|
| **Platform** | PlatformIO `[env:esp32c3]` — targets the real Xteink X4 hardware |
| **Display** | `EspEinkDisplay` — SSD1677 driver via Papyrix's `EInkDisplay` and `GfxRenderer` libraries |
| **Input** | `EspAdcInput` — resistor ladder ADC polling via Papyrix's `InputManager` |
| **Storage** | Currently embedded binary, will be migrated to SD card → flash pipeline in M4 |

Only used once the device arrives. By then, the firmware should be functionally complete from Tiers 1 & 2, needing only calibration of ADC thresholds, display timing, and power management tuning.

---

## Resolved Questions

| Question | Resolution |
|:---|:---|
| **Q1: Papyrix License** | ✅ MIT — confirmed via [LICENSE](https://github.com/bigbag/papyrix-reader/blob/main/LICENSE). Free to fork and extend. |
| **Q2: Wokwi** | ✅ Free tier with license key (stored in `.wokwi`). Internet required for simulation. |
| **Q3: PoC Game** | ✅ Option B — start with a minimal "hello world" ink story, graduate to *The Intercept* once pipeline is validated. |
| **Q4: Device USB** | ✅ Developer edition ordered — USB unlocked for flashing. |
| **Q5: Rendering** | ✅ Papyrix's `GfxRenderer` with Knuth-Plass line breaking and hyphenation is shared across SDL (mock `EInkDisplay`) and real e-ink. The `InkEngine` uses `getRenderer()` for proportional font rendering with halftone support for "old" text. |
| **Q6: SD Card Pins** | ✅ SD card shares the SPI bus with the e-ink display: SCLK=GPIO8, MOSI=GPIO10, MISO=GPIO7, CS=GPIO12. Bus arbitration required. |

---

## Hardware Pin Map (Xteink X4)

| Subsystem | Pin | GPIO |
|:---|:---|:---|
| **E-Ink SPI** | SCLK | GPIO 8 |
| | MOSI | GPIO 10 |
| | CS | GPIO 21 |
| | DC | GPIO 4 |
| | RST | GPIO 5 |
| | BUSY | GPIO 6 |
| **SD Card SPI** | SCLK | GPIO 8 *(shared)* |
| | MOSI | GPIO 10 *(shared)* |
| | MISO | GPIO 7 |
| | CS | GPIO 12 |
| **Input ADC** | ADC_PIN_1 | GPIO 1 |
| | ADC_PIN_2 | GPIO 2 |
| **Power Button** | Digital | GPIO 3 |
| **Battery** | ADC | GPIO 0 |
| **Charge Status** | Digital | GPIO 20 |

---

## Milestone Breakdown

---

### Milestone 0: Project Scaffolding & Toolchain Setup ✅ COMPLETED
**Goal:** Establish the build system, directory structure, and verify all tools work end-to-end.

#### What Was Done
1. Initialized PlatformIO project with three environments: `[env:esp32c3]`, `[env:esp32c3_serial]`, `[env:native]`
2. Cloned & integrated InkCPP as a local library under `lib/inkcpp/`
   - Build flags: `INKCPP_NO_STD`, `INKCPP_NO_RTTI`, `INKCPP_NO_EXCEPTIONS` (for ESP32)
   - Compiles for both `native` and `esp32c3` environments
3. Set up the ink compilation pipeline: `.ink` → (inklecate) → `.json` → (inkcpp_compiler) → `.bin`
4. Created a minimal "hello world" ink story and compiled it to `.bin`
5. Verified InkCPP loads and executes the hello world story

#### Success Criteria
- [x] `pio run -e native` compiles cleanly
- [x] `pio run -e esp32c3` compiles cleanly
- [x] Running the native binary prints "Hello world!" from the ink story

---

### Milestone 1: Naive SRAM PoC — Desktop Simulation ✅ COMPLETED
**Goal:** End-to-end interactive fiction running in a desktop window, proving the InkCPP pipeline works with display + input.

#### What Was Done

##### HAL Architecture
Created platform-agnostic interfaces (`IDisplay`, `IInput`, `IStorage`) with implementations:
- **SDLDisplay**: 480×800 SDL2 window using Papyrix's `EInkDisplay` (in mock/framebuffer mode) and `GfxRenderer` for proportional font rendering
- **SDLInput**: Keyboard mapping via `SDL_PollEvent`
- **SDLStorage**: `std::fopen`/`std::fread` file I/O

##### InkEngine State Machine
Implemented a single `InkEngine` class (instead of separate `GameState`/`TextFormatter` classes) that:
- Loads `.bin` via `IStorage::readFileBinary()` → `story::from_binary(buf, size, false)`
- Runs the game loop: `getline_alloc()` → word-wrap via `GfxRenderer::wrapTextWithHyphenation()` → render
- Handles choices with UP/DOWN cycling, CONFIRM to select
- Supports scrolling with LEFT/RIGHT (page up/down)
- Renders "old" text in halftone and new text in full black
- Auto-scrolls to show new content after each choice

##### Fonts & Typography
- Uses Papyrix's `EpdFont` system with builtin fonts (`reader_medium_2b` for narrative, `ui_12` for choices)
- Knuth-Plass line breaking with hyphenation via `GfxRenderer`
- Selected choice rendered with inverted fill rect

#### Success Criteria
- [x] Interactive fiction playable in SDL2 window with beautiful proportional typography
- [x] UP/DOWN cycles choices, LEFT/RIGHT scrolls, CONFIRM advances narrative
- [x] *The Intercept* runs end-to-end without crashes
- [x] Multiple stories work: hello_world, copper-in-the-grime, item_menu, the_intercept

---

### Milestone 2: ESP32-C3 Cross-Compilation & Memory Validation ✅ COMPLETED
**Goal:** Validate that the firmware compiles and the memory budget works on the real target architecture.

#### What Was Done

##### ESP32 HAL Implementations
- **`EspSerialDisplay`**: VT100/ANSI escape codes for terminal-based UI (used in `esp32c3_serial` env)
- **`EspSerialInput`**: Serial polling for W/S/Enter/B/Q keys
- **`EspLittleFSStorage`**: Accesses the story binary embedded in flash via linker symbols (`_binary_data_the_intercept_bin_start`/`_binary_data_the_intercept_bin_end`), then `memcpy`s into a heap-allocated buffer to simulate file reading

##### Build System
- Story binary (`the_intercept.bin`, 150 KB) embedded via `board_build.embed_txtfiles = data/the_intercept.bin`
- Post-build `merge_firmware.py` script creates a single flashable `firmware-factory.bin` for web flashers
- Three distinct build environments in `platformio.ini`

> [!NOTE]
> **Deviation from plan:** Instead of LittleFS filesystem, we used `embed_txtfiles` to bake the story directly into the firmware binary. This is simpler and avoids filesystem overhead, but means the story cannot be changed without reflashing. Milestone 4 will resolve this by loading from SD card.

#### Success Criteria
- [x] Firmware compiles for `esp32c3` and `esp32c3_serial` targets with no errors
- [x] Heap logging shows sufficient free memory after story load

---

### Milestone 3: Physical Device Bring-Up ✅ COMPLETED
**Goal:** Migrate the Papyrix Reader display and input drivers into the EENK architecture.

#### What Was Done

##### Papyrix Driver Integration
Extracted and integrated the following Papyrix libraries into `lib/`:
- `EInkDisplay` — SSD1677 SPI driver (SCLK=8, MOSI=10, CS=21, DC=4, RST=5, BUSY=6)
- `InputManager` — resistor ladder ADC polling (ADC_PIN_1=GPIO1, ADC_PIN_2=GPIO2, Power=GPIO3)
- `GfxRenderer` — pixel rendering engine with Knuth-Plass line breaking
- `EpdFont`, `ExternalFont` — font loading and rendering
- `Hyphenation` — language-aware hyphenation patterns
- `Utf8`, `ArabicShaper`, `ThaiShaper`, `ScriptDetector` — text processing
- `FsAdapter` — filesystem abstraction (currently stubbed to LittleFS, will be extended for SD)
- `Logging` — debug logging

##### HAL Wrappers
- **`EspEinkDisplay`**: Wraps `EInkDisplay` + `GfxRenderer` with `Portrait` orientation, `FAST_REFRESH` for `present()`, `FULL_REFRESH` for `fullRefresh()`
- **`EspAdcInput`**: Wraps `InputManager`, maps hardware buttons to `ButtonEvent` enum, includes deep sleep on 2-second power button hold

##### Main Entry Point
`main.cpp` uses compile-time `#ifdef` switching:
- `PLATFORM_NATIVE` → SDL HAL
- `PLATFORM_ESP32` + `SERIAL_DEBUG` → Serial HAL (for dev boards)
- `PLATFORM_ESP32` (no `SERIAL_DEBUG`) → E-ink + ADC HAL (for real X4)

#### Hardware Validation Results
- [x] Firmware flashed to physical Xteink X4 via USB
- [x] `[env:esp32c3]` PlatformIO target compiles and runs on device
- [x] E-ink display renders correctly (orientation, typography)
- [x] ADC input (D-Pad) works — button events map correctly
- [ ] ADC voltage thresholds fine-tuning (ongoing)
- [ ] Partial refresh counter tuning for ghosting mitigation (ongoing, deferred to M7)

---

### Milestone 4: SD Card Storage & Flash Memory Mapping — **NEXT UP**
**Goal:** Load ink stories from the SD card, copy to an internal flash partition, and memory-map for zero-SRAM-impact execution. This eliminates the ~150 KB story-size ceiling and enables stories up to 10 MB.

> [!IMPORTANT]
> This is the **critical architecture transition**. After this milestone, stories are no longer baked into the firmware binary. Users can drop `.bin` files onto the SD card, and the device will load them dynamically. This unlocks Milestone 5 (save/resume) and Milestone 6 (game library menu).

#### Architecture Overview

```
┌─────────────┐     SPI Read     ┌──────────────────┐   esp_partition_write   ┌─────────────────┐
│   SD Card   │ ──────────────── │   ESP32-C3 SRAM  │ ──────────────────────── │  ink_cache      │
│  .bin files │   (4KB chunks)   │   (temp buffer)  │    (4KB chunks)         │  flash partition│
└─────────────┘                  └──────────────────┘                         └────────┬────────┘
                                                                                       │
                                                                              esp_partition_mmap
                                                                                       │
                                                                                       ▼
                                                                              ┌─────────────────┐
                                                                              │  const void* ptr │
                                                                              │  (zero-copy)     │
                                                                              └────────┬────────┘
                                                                                       │
                                                                              story::from_binary()
                                                                                       │
                                                                                       ▼
                                                                              ┌─────────────────┐
                                                                              │  InkCPP Runtime  │
                                                                              │  (0 SRAM impact) │
                                                                              └─────────────────┘
```

#### Tasks

##### 4A: Partition Table Engineering

**File:** [partitions.csv](file:///c:/Users/tomgr/dev/eenk/partitions.csv)

Replace the current partition table with one that includes the `ink_cache` data partition:

```csv
# Name,      Type, SubType, Offset,   Size,    Flags
nvs,         data, nvs,     0x9000,   0x5000,
otadata,     data, ota,     0xe000,   0x2000,
app0,        app,  ota_0,   0x10000,  0x300000,
ink_cache,   data, 0x82,    0x310000, 0xA00000,
spiffs,      data, spiffs,  0xD10000, 0x2F0000,
```

Key layout:
- `app0`: 3 MB for firmware (offset 0x10000, plenty for app + InkCPP + Papyrix libs)
- `ink_cache`: **10 MB** for the memory-mapped story binary (offset 0x310000)
- `spiffs`: ~3 MB for future save files, settings, etc. (offset 0xD10000)
- **Total**: 16 MB (matches `board_build.flash_size = 16MB`)
- **Removed**: `app1` (OTA_1) — we don't need dual OTA slots

**Build system change:** Add `board_build.partitions = partitions.csv` to `[env:esp32c3]` and `[env:esp32c3_serial]` in `platformio.ini`.

> [!WARNING]
> Removing the OTA_1 partition means we can no longer do safe A/B OTA updates. This is acceptable since the device updates via SD card or USB flashing anyway. If OTA is needed later, the `ink_cache` can be reduced to 8 MB to fit a second app slot.

##### 4B: SD Card Driver Integration

**New file:** [EspSdStorage.h](file:///c:/Users/tomgr/dev/eenk/src/hal/esp32/EspSdStorage.h)
**New file:** [EspSdStorage.cpp](file:///c:/Users/tomgr/dev/eenk/src/hal/esp32/EspSdStorage.cpp)

Implement `IStorage` using the SD card via SPI. The SD card shares the SPI bus with the e-ink display, so we must handle bus arbitration.

**SPI Pin Configuration:**
- SCLK: GPIO 8 (shared with e-ink)
- MOSI: GPIO 10 (shared with e-ink)
- MISO: GPIO 7 (SD card only)
- CS: GPIO 12 (SD card chip select)

**Implementation details:**
```cpp
#pragma once
#include "hal/IStorage.h"
#include <SdFat.h>

// SD card SPI pins (shared bus with e-ink display)
#define SD_CS_PIN    12
#define SD_MISO_PIN  7
// SCLK (GPIO8) and MOSI (GPIO10) are shared with e-ink, already configured

class EspSdStorage : public IStorage
{
public:
    EspSdStorage();
    ~EspSdStorage() override;

    bool begin();  // Initialize SD card — call AFTER SPI.begin()

    const unsigned char* readFileBinary(const char* path, std::size_t* outSize) override;
    void freeBuffer(const unsigned char* buf) override;
    bool fileExists(const char* path) override;

    // SD-specific: iterate .bin files for game library (M6)
    // int listBinFiles(char filenames[][64], int maxFiles);

private:
    SdFat _sd;
    bool  _initialized = false;
};
```

**Key implementation notes:**
1. Use `SdFat` library (already a dependency via Papyrix's `FsAdapter`)
2. Initialize with `_sd.begin(SD_CS_PIN, SD_SCK_MHZ(20))` — start conservative, can tune up to 40 MHz
3. `readFileBinary()` allocates a heap buffer with `malloc()`, reads the file in chunks, returns the buffer
4. `freeBuffer()` calls `free()`
5. `fileExists()` uses `_sd.exists(path)`
6. **Bus arbitration**: The SPI bus is shared between e-ink and SD card. Both are accessed via different CS pins, so standard SPI CS selection handles arbitration. The `SdFat` library manages transactions internally. However, avoid simultaneous access by not reading SD during display refresh.

**Library dependency:** Add `SdFat` to `lib_deps` in platformio.ini for ESP32 environments:
```ini
lib_deps =
    FS
    LittleFS
    greiman/SdFat@^2.2.0
```

> [!NOTE]
> The existing `FsAdapter/SDCardManager.h` in `lib/` is currently stubbed to use LittleFS. We will NOT modify it — instead, we create our own `EspSdStorage` that directly uses `SdFat` for reading story binaries. The `FsAdapter` can remain as-is for Papyrix's font loading system, which will eventually need SD access too (M6+).

##### 4C: Flash Cache Manager

**New file:** [FlashCache.h](file:///c:/Users/tomgr/dev/eenk/src/hal/esp32/FlashCache.h)
**New file:** [FlashCache.cpp](file:///c:/Users/tomgr/dev/eenk/src/hal/esp32/FlashCache.cpp)

This component handles the SD → flash bridge and memory mapping lifecycle.

```cpp
#pragma once
#include <cstddef>
#include <cstdint>

// Forward declarations for ESP-IDF types
struct esp_partition_t;
typedef uint32_t esp_partition_mmap_handle_t;

class FlashCache
{
public:
    FlashCache();
    ~FlashCache();

    /**
     * Load a story binary from SD card into the ink_cache flash partition,
     * then memory-map it for zero-copy access.
     *
     * @param sdStorage  The SD card storage to read from
     * @param sdPath     Path to the .bin file on SD card
     * @param outPtr     Receives the memory-mapped pointer to the story data
     * @param outSize    Receives the story binary size in bytes
     * @return true on success
     *
     * If the file is already cached (hash match), skips the SD→flash copy.
     * Caller should NOT free the returned pointer — it is memory-mapped.
     */
    bool loadStory(const char* sdPath,
                   const unsigned char** outPtr,
                   std::size_t* outSize);

    /** Unmap the current story from memory. Call before loading a new story. */
    void unload();

    /** Returns true if a story is currently memory-mapped. */
    bool isLoaded() const { return _mapped; }

    /** Progress callback for UI during flash writes. 0.0 to 1.0. */
    using ProgressCallback = void(*)(float progress);
    void setProgressCallback(ProgressCallback cb) { _progressCb = cb; }

private:
    const esp_partition_t*        _partition = nullptr;
    esp_partition_mmap_handle_t   _mmapHandle = 0;
    const void*                   _mappedPtr = nullptr;
    bool                          _mapped = false;
    std::size_t                   _cachedSize = 0;
    uint32_t                      _cachedHash = 0;
    ProgressCallback              _progressCb = nullptr;

    bool findPartition();
    bool writeToFlash(const unsigned char* data, std::size_t size);
    bool mapFromFlash(std::size_t size);
    uint32_t computeHash(const unsigned char* data, std::size_t size);
    bool loadHashFromNvs(uint32_t* outHash, std::size_t* outSize);
    void saveHashToNvs(uint32_t hash, std::size_t size);
};
```

**Implementation walkthrough:**

1. **`findPartition()`**: Uses `esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x82, "ink_cache")` to locate the flash partition.

2. **`loadStory()`** sequence:
   a. Read the entire `.bin` file from SD card into a temporary SRAM buffer via `EspSdStorage::readFileBinary()`
   b. Compute a CRC32 hash of the buffer
   c. Check NVS for a stored hash + size from the previous load
   d. **If hash matches**: Skip flash write, just memory-map at the existing offset
   e. **If hash differs**: Erase the partition region with `esp_partition_erase_range()`, write in 4 KB chunks with `esp_partition_write()`, calling the progress callback after each chunk
   f. Store the new hash + size in NVS
   g. Call `mapFromFlash()` to get the `const void*` pointer
   h. **Free the temporary SRAM buffer** — the mmap'd pointer is now the only reference
   i. Set `*outPtr` and `*outSize`

3. **`mapFromFlash()`**: 
   ```cpp
   esp_err_t err = esp_partition_mmap(
       _partition, 0, size,
       ESP_PARTITION_MMAP_DATA,  // Note: not SPI_FLASH_MMAP_DATA
       &_mappedPtr, &_mmapHandle);
   ```

4. **`unload()`**: Calls `esp_partition_munmap(_mmapHandle)` and resets state.

5. **`computeHash()`**: Simple CRC32 over the file contents. We don't need cryptographic security, just change detection. Use the ROM CRC32 function or a simple implementation.

6. **NVS storage**: Use `nvs_open("ink_cache", ...)` → `nvs_set_u32("hash", ...)` / `nvs_set_u32("size", ...)`.

**Required includes:**
```cpp
#include "esp_partition.h"
#include "esp_spi_flash.h"  // for esp_partition_mmap
#include "nvs_flash.h"
#include "nvs.h"
```

> [!IMPORTANT]
> **Memory budget during loading**: The temporary SRAM buffer for reading from SD will consume up to 150 KB (for The Intercept). This is acceptable because:
> 1. The buffer is freed immediately after writing to flash
> 2. The InkEngine is not yet initialized, so no runner/globals are allocated
> 3. Future optimization: read SD in streaming 4 KB chunks directly to flash, avoiding the full SRAM copy (see 4F below)

##### 4D: IStorage Interface Extension & InkEngine Integration

**Modify:** [IStorage.h](file:///c:/Users/tomgr/dev/eenk/src/hal/IStorage.h)

The `IStorage` interface remains unchanged — the key change is in how `InkEngine::loadStory()` is called on ESP32. We introduce a new method to load from a memory-mapped pointer instead of via `IStorage`.

**Modify:** [InkEngine.h](file:///c:/Users/tomgr/dev/eenk/src/engine/InkEngine.h)

Add a new overload:
```cpp
/**
 * Load a story from a pre-existing memory buffer (e.g., memory-mapped flash).
 * The buffer is NOT owned by InkEngine — caller is responsible for its lifetime.
 * @param data   Pointer to the story binary data
 * @param size   Size of the binary in bytes
 * @return true on success
 */
bool loadStoryFromMemory(const unsigned char* data, std::size_t size);
```

**Modify:** [InkEngine.cpp](file:///c:/Users/tomgr/dev/eenk/src/engine/InkEngine.cpp)

```cpp
bool InkEngine::loadStoryFromMemory(const unsigned char* data, std::size_t size)
{
    // NOTE: _storyBuf is NOT set — we don't own this memory (it's mmap'd)
    _story = ink::runtime::story::from_binary(data, size, false);
    if (!_story) {
        fprintf(stderr, "[InkEngine] story::from_binary() failed (mmap)\n");
        return false;
    }
    _globals = _story->new_globals();
    _runner  = _story->new_runner(_globals);
    // ... same font init as loadStory() ...
    _state = State::RUNNING_TEXT;
    return true;
}
```

Also update the destructor to only call `_storage.freeBuffer()` if `_storyBuf` is non-null (already the case).

**Modify:** [main.cpp](file:///c:/Users/tomgr/dev/eenk/src/main.cpp) (ESP32 section)

Replace the current embedded binary loading with the SD→flash→mmap pipeline:

```cpp
// In setup():
#include "hal/esp32/EspSdStorage.h"
#include "hal/esp32/FlashCache.h"

EspSdStorage* sdStorage = nullptr;
FlashCache* flashCache = nullptr;

void setup() {
    Serial.begin(115200);
    // ... display/input init ...

    sdStorage = new EspSdStorage();
    if (!sdStorage->begin()) {
        Serial.println("ERROR: SD card init failed!");
        return;
    }

    flashCache = new FlashCache();
    flashCache->setProgressCallback([](float p) {
        Serial.printf("Loading: %.0f%%\n", p * 100);
        // TODO (M6): Show progress bar on e-ink display
    });

    const char* storyPath = "/eenk/the_intercept.bin";  // SD card path
    const unsigned char* mappedPtr = nullptr;
    std::size_t mappedSize = 0;

    if (!flashCache->loadStory(storyPath, &mappedPtr, &mappedSize)) {
        Serial.println("ERROR: Failed to load story from SD card!");
        // Fallback: try embedded binary
        // ... existing code ...
        return;
    }

    engine = new InkEngine(*display, *input, *sdStorage);
    if (!engine->loadStoryFromMemory(mappedPtr, mappedSize)) {
        Serial.println("ERROR: Failed to initialize InkCPP from flash map!");
        return;
    }

    Serial.printf("Free heap after mmap load: %u bytes\n", ESP.getFreeHeap());
}
```

> [!NOTE]
> The native (SDL) path remains unchanged — it still uses `SDLStorage::readFileBinary()` to load into heap memory. Memory mapping is an ESP32-only optimization.

##### 4E: platformio.ini Updates

**Modify:** [platformio.ini](file:///c:/Users/tomgr/dev/eenk/platformio.ini)

Changes needed for both `esp32c3` and `esp32c3_serial` environments:

1. **Add partition table reference:**
   ```ini
   board_build.partitions = partitions.csv
   ```

2. **Remove embedded story binary** (it will come from SD now):
   ```ini
   ; REMOVE: board_build.embed_txtfiles = data/the_intercept.bin
   ```

3. **Add SdFat dependency:**
   ```ini
   lib_deps =
       FS
       LittleFS
       greiman/SdFat@^2.2.0
   ```

4. **Initialize NVS flash** — add build flag:
   ```ini
   build_flags =
       ...
       -DCONFIG_NVS_ENCRYPTION=0
   ```

5. **Keep `esp32c3_serial` as a fallback** that can still use the embedded binary if SD is unavailable (for dev boards without SD slots). Guard with `#ifdef HAS_SD_CARD` or similar.

##### 4F: Streaming Optimization (Optional, Can Defer)

For very large stories (>1 MB), reading the entire file into SRAM before writing to flash is wasteful. An optimized version would stream 4 KB chunks directly from SD to flash:

```cpp
bool FlashCache::loadStoryStreaming(const char* sdPath, ...) {
    FsFile file = _sd.open(sdPath, O_RDONLY);
    size_t fileSize = file.fileSize();
    
    esp_partition_erase_range(_partition, 0, alignUp(fileSize, 4096));
    
    uint8_t buf[4096];
    size_t offset = 0;
    while (offset < fileSize) {
        size_t toRead = min(sizeof(buf), fileSize - offset);
        file.read(buf, toRead);
        // Pad to 4-byte boundary for esp_partition_write
        size_t toWrite = (toRead + 3) & ~3;
        if (toWrite > toRead) memset(buf + toRead, 0xFF, toWrite - toRead);
        esp_partition_write(_partition, offset, buf, toWrite);
        offset += toRead;
        if (_progressCb) _progressCb((float)offset / fileSize);
    }
    file.close();
    // Then compute hash over flash content or file content...
}
```

This reduces peak SRAM usage from `fileSize` to just 4 KB, essential for stories larger than ~150 KB. **Recommend implementing this from the start** rather than the two-step "read all then write all" approach.

> [!IMPORTANT]
> **Hash computation with streaming**: If we stream directly, we can't compute the hash before writing. Two options:
> 1. Compute hash in a first pass over the SD file (read-only), then stream-write if hash differs. Two SD reads but minimal SRAM.
> 2. Always write to flash, compute hash after write from the mmap'd pointer, store for next comparison. One SD read but always writes (wearing flash).
> 
> **Recommended: Option 1** — the SD read is fast (~4 seconds for 1 MB at 20 MHz SPI), and we save flash write cycles.

##### 4G: Native (SDL) Simulation of SD Card

For desktop development, the existing `SDLStorage` already reads from the local filesystem, which simulates SD card behavior. No changes needed for M4 on the native tier.

However, to test the `FlashCache` concept on desktop, we could add a `MockFlashCache` that just uses `mmap()` on the host OS — but this is **not required** for M4. The native path will continue loading stories into heap memory via `loadStory()`.

#### SD Card File Layout Convention

Stories should be placed in an `/eenk/` directory on the SD card root:

```
SD Card (FAT32)
├── eenk/
│   ├── the_intercept.bin
│   ├── copper-in-the-grime.bin
│   ├── my_game.bin
│   └── saves/               (M5: save files go here)
│       ├── the_intercept.snap
│       └── my_game.snap
├── books/                    (existing Papyrix EPUB storage)
└── ...
```

#### New/Modified Files Summary

| Action | File | Description |
|:---|:---|:---|
| **MODIFY** | `partitions.csv` | Add `ink_cache` (10 MB), remove `app1` (OTA_1) |
| **MODIFY** | `platformio.ini` | Add `board_build.partitions`, SdFat dep, remove embedded story |
| **NEW** | `src/hal/esp32/EspSdStorage.h` | SD card IStorage implementation header |
| **NEW** | `src/hal/esp32/EspSdStorage.cpp` | SD card IStorage implementation |
| **NEW** | `src/hal/esp32/FlashCache.h` | SD→flash bridge + mmap manager header |
| **NEW** | `src/hal/esp32/FlashCache.cpp` | SD→flash bridge + mmap manager implementation |
| **MODIFY** | `src/engine/InkEngine.h` | Add `loadStoryFromMemory()` overload |
| **MODIFY** | `src/engine/InkEngine.cpp` | Implement `loadStoryFromMemory()` |
| **MODIFY** | `src/main.cpp` | ESP32 section: use EspSdStorage + FlashCache pipeline |

#### Verification Plan

##### Automated Build Verification
```bash
pio run -e native          # Ensure native still compiles and works
pio run -e esp32c3         # Ensure new partition table + SD code compiles
pio run -e esp32c3_serial  # Ensure serial debug env still works
```

##### ESP32 Functional Testing (on device or Wokwi)
1. **SD card detection**: Verify SD card initializes successfully, print file listing
2. **Flash write**: Copy `the_intercept.bin` (150 KB) from SD to `ink_cache` partition
3. **Memory map**: Verify `esp_partition_mmap()` succeeds, print mapped pointer address
4. **Story execution**: Play through at least one branch of The Intercept
5. **Hash caching**: Reboot → verify story loads instantly (flash write skipped)
6. **Heap validation**: `esp_get_free_heap_size()` should show ~200+ KB during gameplay (vs. ~84 KB with SRAM loading)

##### Stress Testing (on device)
1. Load progressively larger stories: hello_world (5 KB) → copper-in-the-grime (33 KB) → the_intercept (150 KB)
2. Switch between stories (unmap → load new → map) 10 times — verify no memory leaks
3. Pull SD card during load — verify graceful error handling, no flash corruption

#### Success Criteria
- [ ] Stories loaded from SD card `/eenk/` directory
- [ ] Flash write < 5 seconds for The Intercept (150 KB)
- [ ] Hash-based caching works: second boot loads instantly
- [ ] `esp_get_free_heap_size()` shows ~200+ KB during gameplay
- [ ] Game plays identically to the embedded-binary version
- [ ] Graceful error messages for: SD not inserted, file not found, partition full

#### Benchmarks to Capture
| Metric | Embedded Binary (M2) | Flash Map (M4) |
|:---|:---|:---|
| Free heap during gameplay | ~84 KB | ~200+ KB |
| Max story size supported | ~150 KB | ~10 MB |
| First load time (SD→flash) | N/A | < 5s for 150 KB |
| Subsequent load time (hash hit) | Instant | < 100ms |
| Execution latency per choice | ~0ms | ~0ms (hardware cached) |

#### Estimated Effort: 3–5 days

---

### Milestone 5: Save/Resume System
**Goal:** Implement persistent game state so the device can sleep/resume mid-story and support game switching.

> [!NOTE]
> This milestone depends on M4 (SD card access) being complete, since save files are stored on the SD card.

#### Architecture Overview

InkCPP provides a comprehensive snapshot API for serializing and restoring game state. Key API surface (from `inkcpp/include/snapshot.h`, `story.h`, `runner.h`):

**Saving:**
```cpp
// 1. Create snapshot from runner (captures ALL runners sharing the same globals)
ink::runtime::snapshot* snap = runner->create_snapshot();
// Internally delegates to globals->create_snapshot() — serializes globals + all runners

// 2. Get raw binary blob for persistence
const unsigned char* data = snap->get_data();
size_t len = snap->get_data_len();

// 3. Write to file (via our IStorage)
storage.writeFileBinary("/eenk/saves/story_name.snap", data, len);

// 4. Clean up
delete snap;
```

**Restoring:**
```cpp
// 1. Read saved blob from file
const unsigned char* data;
size_t len;
storage.readFileBinary("/eenk/saves/story_name.snap", &data, &len);

// 2. Reconstruct snapshot object
ink::runtime::snapshot* snap = ink::runtime::snapshot::from_binary(data, len, true);
// freeOnDestroy=true — snapshot takes ownership of the buffer

// 3. Reconstruct globals and runner from snapshot
ink::runtime::globals globals = story->new_globals_from_snapshot(*snap);
ink::runtime::runner runner = story->new_runner_from_snapshot(*snap, globals, 0);
// runner_id=0 for the first (and usually only) runner

// 4. Clean up snapshot object
delete snap;

// 5. Continue execution — runner is at the exact same story position
```

The snapshot captures:
- Current instruction pointer position in the story
- All global variable values
- The evaluation stack state
- The call stack and all runner states

> [!NOTE]
> `snapshot::from_binary(data, len, false)` also supports non-owning buffers (same semantics as `story::from_binary`). This means snapshots could theoretically be memory-mapped too, but since they're small (<50 KB typically) and we need to write them, SRAM is fine.

Save files (`.snap`) are stored on the SD card alongside the story binaries.

#### Tasks

##### 5A: IStorage Interface Extension

**Modify:** [IStorage.h](file:///c:/Users/tomgr/dev/eenk/src/hal/IStorage.h)

Add write capability:
```cpp
/**
 * Write a buffer to a file, creating or overwriting it.
 * @return true on success
 */
virtual bool writeFileBinary(const char* path, const unsigned char* data, std::size_t size) = 0;
```

Implement in both `SDLStorage` and `EspSdStorage`.

##### 5B: SaveManager Component

**New file:** `src/engine/SaveManager.h`
**New file:** `src/engine/SaveManager.cpp`

```cpp
class SaveManager {
public:
    SaveManager(IStorage& storage);
    
    /**
     * Save the current runner state to a .snap file on SD card.
     * Path is derived from the story path: /eenk/saves/<story_name>.snap
     */
    bool saveState(ink::runtime::runner& runner, const char* storyPath);
    
    /**
     * Check if a save file exists for the given story.
     */
    bool hasSave(const char* storyPath);
    
    /**
     * Load a saved snapshot. Returns the raw snapshot data.
     * Caller uses story->new_runner_from_snapshot() with this data.
     */
    bool loadState(const char* storyPath, 
                   const unsigned char** outData, std::size_t* outSize);
    
    /** Delete a save file. */
    bool deleteSave(const char* storyPath);

private:
    IStorage& _storage;
    void buildSavePath(const char* storyPath, char* outPath, size_t maxLen);
};
```

##### 5C: InkEngine Save/Resume Integration

**Modify:** `InkEngine.h` / `InkEngine.cpp`

1. Add `SaveManager` member
2. On `ButtonEvent::BACK`: call `saveState()` instead of showing stub message
3. On `loadStory()` / `loadStoryFromMemory()`: check for existing save, offer to resume
4. Add auto-save before deep sleep (triggered by power button hold)

##### 5D: Deep Sleep with Save

**Modify:** `EspAdcInput.cpp`

Before entering deep sleep on power button hold, signal the engine to auto-save:
```cpp
// Instead of immediately sleeping:
// 1. Signal InkEngine to save state
// 2. Wait for save completion
// 3. Then enter deep sleep
```

On wake from deep sleep:
```cpp
// In setup():
// 1. Check wake reason via esp_sleep_get_wakeup_cause()
// 2. If GPIO wake: load the last-played story + restore from save
// 3. Skip the story selection (future M6 menu)
```

##### 5E: Resume Flow

After wake or reboot:
1. Check NVS for "last played story" path
2. If exists, check if save file exists on SD card
3. If both exist: auto-load story → restore snapshot → resume at last position
4. If no save: start story from beginning

#### Verification Plan
- Save mid-story → reboot → verify resume at exact same point
- Save → play further → save again → verify overwrite works
- Save → switch stories → switch back → verify old save still works
- Deep sleep → wake → verify auto-resume
- Delete save file from SD → verify graceful "start from beginning"

#### Success Criteria
- [ ] Save/load works reliably with The Intercept
- [ ] Deep sleep + wake resumes at the correct story position
- [ ] Save file size is reasonable (< 50 KB for The Intercept)
- [ ] Auto-save on power button hold works

#### Estimated Effort: 3–4 days

---

### Milestone 6: OS Shell — Menu, Settings & Dual-Boot Architecture
**Goal:** Build an OS-level shell with a game library browser, settings screen, and a dual-boot architecture that separates WiFi-enabled menu mode from WiFi-disabled ink runtime mode.

#### Architecture Overview: Dual-Boot System

The ESP32-C3 has only 400 KB of SRAM. WiFi alone consumes ~80 KB. Running WiFi while playing a game leaves dangerously little memory for the InkCPP runtime. Solution: **two operating modes** that reboot between them.

```
┌──────────────────────────────────────────────────┐
│                   BOOT                            │
│   Check NVS "boot_mode" flag                      │
│   ┌──────────────────┐  ┌──────────────────────┐  │
│   │  MODE_MENU       │  │  MODE_INK_RUNTIME    │  │
│   │  WiFi ON         │  │  WiFi OFF            │  │
│   │  ~300 KB free     │  │  ~380 KB free        │  │
│   │                  │  │                      │  │
│   │  • Game library  │  │  • InkEngine         │  │
│   │  • Settings      │  │  • Full story exec   │  │
│   │  • WiFi transfer │  │  • Save/resume       │  │
│   │  • About screen  │  │  • Deep sleep/wake   │  │
│   └──────────────────┘  └──────────────────────┘  │
│         │                         │                │
│   "Launch game"              "Exit to menu"        │
│   → Set NVS boot_mode       → Set NVS boot_mode   │
│   → Set NVS story_path      → ESP.restart()        │
│   → ESP.restart()                                   │
└──────────────────────────────────────────────────┘
```

#### Tasks

##### 6A: Boot Mode Manager

**New file:** `src/os/BootManager.h` / `.cpp`

```cpp
enum class BootMode { MENU, INK_RUNTIME };

class BootManager {
public:
    static BootMode getBootMode();        // Read from NVS
    static void setBootMode(BootMode m);  // Write to NVS
    static void setStoryPath(const char* path);
    static bool getStoryPath(char* outPath, size_t maxLen);
    static void reboot();                 // ESP.restart()
};
```

##### 6B: Game Library UI

**New file:** `src/ui/GameLibrary.h` / `.cpp`

Renders a list of `.bin` files found in `/eenk/` on the SD card:
- Displays file names (with `.bin` extension stripped)
- Shows save indicator (✓ = has save file)
- Shows file size
- UP/DOWN to scroll, CONFIRM to launch, BACK to go to settings
- On CONFIRM: set NVS boot_mode=INK_RUNTIME + story_path, reboot

Uses the same `GfxRenderer` + `EpdFont` system for consistent typography.

##### 6C: Settings Screen

**New file:** `src/ui/SettingsView.h` / `.cpp`

Simple settings menu:
- **Brightness** (if e-ink supports contrast levels)
- **Refresh mode**: Fast (more ghosting) vs. Quality (slower, less ghosting)
- **Full refresh interval**: Every 10/15/20/30 partial updates
- **WiFi file transfer**: Enable/disable a simple HTTP upload server
- **About**: Show firmware version, free heap, battery %, story count
- **Delete all saves**: Confirmation dialog

Settings stored in NVS.

##### 6D: WiFi File Transfer (Optional)

When in Menu mode, optionally start a minimal HTTP server for uploading `.bin` files:
- Connect to user's WiFi (credentials via serial or pre-configured)
- Serve a simple upload page at the device's IP
- Accept `.bin` file uploads, save to SD card `/eenk/`
- Show IP address and status on the e-ink display

> [!NOTE]
> This is a nice-to-have. Users can always just remove the SD card and copy files from a computer. Defer if it adds too much complexity.

##### 6E: Main Entry Point Refactor

**Modify:** [main.cpp](file:///c:/Users/tomgr/dev/eenk/src/main.cpp)

```cpp
void setup() {
    BootMode mode = BootManager::getBootMode();
    
    if (mode == BootMode::INK_RUNTIME) {
        // WiFi OFF — maximum memory for InkCPP
        // Load story from NVS path, resume from save if available
        setupInkRuntime();
    } else {
        // WiFi available — show menu
        // Scan SD card for .bin files, show game library
        setupMenu();
    }
}
```

#### Verification Plan
- Boot into menu → see list of stories from SD card
- Select story → verify reboot into ink runtime → story plays
- Exit story (BACK long press) → verify reboot into menu
- Verify WiFi is OFF during ink runtime (check heap usage)
- Settings persist across reboots

#### Estimated Effort: 5–7 days

---

### Milestone 7: Polish, Power Management & Production Readiness
**Goal:** Production-quality firmware ready for daily use.

#### Tasks

##### 7A: Power Management
- Deep sleep between user inputs during gameplay (ink engine is idle while waiting for choice)
- Wake on GPIO interrupt from power button
- Battery level monitoring via GPIO 0 ADC
- Low battery warning (< 10%) on e-ink display
- Target: weeks of standby, 8+ hours active play on 650 mAh battery

##### 7B: Display Quality
- Tune partial refresh counter (force full refresh every 15 partial updates)
- Power down display controller after each refresh (sunlight fading fix)
- Consider different refresh modes for narrative text (quality) vs. choice scrolling (fast)

##### 7C: Typography Polish
- Verify Knuth-Plass line breaking looks correct with various story text
- Test with longer stories and different text patterns
- Choice rendering with clear visual hierarchy

##### 7D: Error Handling
- Graceful handling of corrupted story files
- SD card removal during play (save state first if possible)
- Flash write failure recovery
- Out-of-memory recovery (shouldn't happen after M4, but defensive)

##### 7E: Firmware Update via SD Card
- Support placing `firmware.bin` on SD card root for self-update
- On boot, check for update file, flash to `app0`, delete file, reboot
- Show update progress on e-ink display

#### Estimated Effort: 5–7 days

---

## Project Directory Structure (Current + Planned)

```
eenk/
├── platformio.ini              # Three environments: esp32c3, esp32c3_serial, native
├── partitions.csv              # Custom partition table with ink_cache (M4)
├── merge_firmware.py           # Post-build: merge into single flashable binary
├── link_flags.py               # Pre-build: SDL2 linker flags for native env
├── hash.py                     # Utility script
├── stories/                    # Source .ink files and compiled .bin files
│   ├── hello_world.ink/.bin
│   ├── copper-in-the-grime.ink/.bin
│   ├── item_menu.ink/.bin
│   └── the_intercept.ink/.bin
├── data/
│   └── the_intercept.bin       # Embedded binary for esp32c3 (removed in M4)
├── tools/
│   ├── eenky/                  # Unified compiler tool project
│   └── compile_ink.py          # .ink → .json → .bin pipeline script
├── lib/
│   ├── inkcpp/                 # InkCPP runtime source (git submodule)
│   ├── EInkDisplay/            # Papyrix SSD1677 driver
│   ├── InputManager/           # Papyrix resistor ladder ADC
│   ├── GfxRenderer/            # Papyrix pixel rendering + Knuth-Plass
│   ├── EpdFont/                # Papyrix font loader
│   ├── ExternalFont/           # Papyrix external font support
│   ├── Hyphenation/            # Papyrix language-aware hyphenation
│   ├── FsAdapter/              # Papyrix filesystem abstraction
│   ├── Logging/                # Papyrix debug logging
│   ├── Utf8/                   # UTF-8 processing
│   ├── ArabicShaper/           # Arabic text shaping
│   ├── ThaiShaper/             # Thai text shaping
│   └── ScriptDetector/         # Script/language detection
├── src/
│   ├── main.cpp                # Entry point (env-specific via #ifdef)
│   ├── hal/
│   │   ├── IDisplay.h          # Abstract display interface
│   │   ├── IInput.h            # Abstract input interface (ButtonEvent enum)
│   │   ├── IStorage.h          # Abstract storage interface
│   │   ├── sdl/
│   │   │   ├── SDLDisplay.h/.cpp    # SDL2 + mock EInkDisplay + GfxRenderer
│   │   │   ├── SDLInput.h/.cpp      # Keyboard → ButtonEvent mapping
│   │   │   ├── SDLStorage.h/.cpp    # std::fopen/fread
│   │   │   ├── font_8x8.h          # Fallback bitmap font (unused now)
│   │   │   └── mock/               # Mock headers for native compilation
│   │   └── esp32/
│   │       ├── EspEinkDisplay.h/.cpp    # SSD1677 via Papyrix EInkDisplay
│   │       ├── EspAdcInput.h/.cpp       # Resistor ladder via InputManager
│   │       ├── EspSerialDisplay.h/.cpp  # VT100 terminal (serial debug)
│   │       ├── EspSerialInput.h/.cpp    # Serial.read() polling
│   │       ├── EspLittleFSStorage.h/.cpp # Embedded binary access (M2, legacy)
│   │       ├── EspSdStorage.h/.cpp      # [NEW M4] SD card via SdFat
│   │       └── FlashCache.h/.cpp        # [NEW M4] SD→flash→mmap manager
│   ├── engine/
│   │   ├── InkEngine.h/.cpp    # Story execution state machine
│   │   └── SaveManager.h/.cpp  # [NEW M5] Save/load state to SD card
│   ├── os/                     # [NEW M6]
│   │   └── BootManager.h/.cpp  # Dual-boot mode switching
│   └── ui/                     # [NEW M6]
│       ├── GameLibrary.h/.cpp  # SD card story browser
│       └── SettingsView.h/.cpp # Settings menu
├── build/                      # InkCPP compiler build outputs
├── build_inkcpp/               # InkCPP compiler build directory
├── docs/
│   └── intial research.md      # Original architecture research
├── include/                    # PlatformIO include directory
└── test/                       # Unit and integration tests
```

---

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|:---|:---|:---|:---|
| InkCPP won't compile for ESP32-C3 RISC-V | **High** | ~~Low~~ **Resolved** | ✅ Resolved at M0 — compiles cleanly |
| SRAM too tight even for small stories | Medium | ~~Medium~~ **Resolved** | ✅ Resolved by M4 flash mapping architecture |
| Papyrix architecture changed since research | Medium | ~~Low~~ **Resolved** | ✅ Resolved — drivers extracted and integrated |
| USB port locked on purchased device | **High** | Medium | Developer edition ordered; SD-card flash as fallback |
| SD card shared SPI bus causes conflicts | Medium | Medium | Use separate CS pins; avoid SD access during display refresh |
| Flash partition mmap fails on ESP32-C3 | **High** | Low | Well-documented ESP-IDF API; test early in M4 |
| InkCPP snapshot API doesn't serialize correctly | Medium | Low | Test save/restore with hello_world first; verify round-trip |
| SdFat library conflicts with Papyrix FsAdapter | Medium | Medium | Use separate instances; don't modify FsAdapter |
| Large stories (>1 MB) exhaust SRAM during SD→flash copy | Medium | Medium | Use streaming 4 KB chunk transfer (4F) instead of full-file read |
| Flash write wear | Low | Low | ~100K erase cycles = decades of use; hash-based skip reduces writes |

---

## Verification Plan

### Automated Tests
- **Build verification**: `pio run -e native`, `pio run -e esp32c3`, `pio run -e esp32c3_serial`
- **Native integration**: Run native binary with multiple stories, verify no crashes
- **Memory profiling** (ESP32): `esp_get_free_heap_size()` at init, after story load, during gameplay, at peak choice depth

### Manual Verification
- **Desktop PoC** (M1): Visual inspection of SDL window — text readability, choice highlighting, scroll behavior ✅
- **Device testing** (M3): Physical button responsiveness, e-ink refresh quality, ghosting threshold tuning
- **SD card loading** (M4): Insert SD card with stories → verify auto-detection → play game
- **Save/resume** (M5): Save mid-story → reboot → verify resume at exact position
- **Menu system** (M6): Browse game library → launch → exit → return to menu
- **Stress testing** (M4+): Load progressively larger stories (5 KB → 33 KB → 150 KB → synthetic 1 MB+)

---

## Recommended Execution Order

```mermaid
gantt
    title EENK Development Timeline
    dateFormat  YYYY-MM-DD
    axisFormat  %b %d

    section Completed
    M0: Scaffolding & Toolchain       :done, m0, 2026-06-07, 2d
    M1: Desktop PoC (SDL/GfxRenderer) :done, m1, after m0, 5d
    M2: ESP32-C3 Cross-Compile        :done, m2, after m1, 3d
    M3: Physical Device Bring-Up      :done, m3, after m2, 5d

    section In Progress
    M4: SD Card & Flash Mapping       :active, m4, 2026-06-16, 5d
    M5: Save/Resume System            :m5, after m4, 4d

    section Future
    M6: OS Shell (Menu/Settings)      :m6, after m5, 7d
    M7: Polish & Production           :m7, after m6, 7d
```

> [!NOTE]
> Milestones 0–3 are **fully complete**, including hardware validation on the physical Xteink X4. M4 is the next actionable milestone and unblocks everything that follows.

---

## Side Projects & Enhancements

### Unified Story Compiler Tool (✅ COMPLETED)
We have successfully built a dedicated Electron/Vite desktop application that combines `inklecate` and `inkcpp_cl` into a single, seamless pipeline. 

Authors using editors like Inky can now conveniently export ready-to-use `.bin` files for the Xteink X4 with a single drag-and-drop action, vastly improving the developer and author experience. The tool is available in `tools/eenk-compiler` as both a beautiful GUI and a headless CLI.
