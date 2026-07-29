# Backlog for eenk.

* See tools/eenky/TODO.md for the eenky TODOs.

## Bugs

* Investigate why The Intercept eventually crashes when using an SD font but not a builtin font. OOM ? is it related to history stack? 

<details>

<summary>Crash logs (from when the crash occurs)</summary>

```
[InkEngine] Font: SD family 'serif-small'
Save loaded successfully!
Free heap after load: 93672 bytes
[InkEngine] fullRefresh (interval reached, 20)

abort() was called at PC 0x420402e3 on core 0
Core  0 register dump:
MEPC    : 0x403825fc  RA      : 0x403879f8  SP      : 0x3fc9ee50  GP      : 0x3fc8ea00
TP      : 0x3fa15cd8  T0      : 0x37363534  T1      : 0x7271706f  T2      : 0x33323130
S0/FP   : 0x3fc9ee7c  S1      : 0x3fc9ee7c  A0      : 0x3fc9ee88  A1      : 0x3fc9ee6a
A2      : 0x00000000  A3      : 0x3fc9eeb5  A4      : 0x00000001  A5      : 0x3fc96000
A6      : 0x7a797877  A7      : 0x76757473  S2      : 0x3fcd1cbc  S3      : 0x3fc9f004
S4      : 0x3fcd9bc8  S5      : 0x3fcd1cd4  S6      : 0x3fcd1cd4  S7      : 0x3fcd9be0
S8      : 0x3fcb2da4  S9      : 0x00000001  S10     : 0x000001d0  S11     : 0x000000b2
T3      : 0x6e6d6c6b  T4      : 0x6a696867  T5      : 0x66656463  T6      : 0x62613938
MSTATUS : 0x00001801  MTVEC   : 0x40380001  MCAUSE  : 0x00000007  MTVAL   : 0x00000000
MHARTID : 0x00000000

Stack memory:
3fc9ee50: 0x00000001 0x00000001 0x3fc9ee68 0x4038cbaa 0x00000000 0x00000000 0x3fc90030 0x3fc8fe28
3fc9ee70: 0x3fc9ee7c 0x3fc8fe44 0x3fc9ee68 0x34303234 0x33653230 0x3fcd1c00 0x726f6261 0x20292874
3fc9ee90: 0x20736177 0x6c6c6163 0x61206465 0x43502074 0x34783020 0x30343032 0x20336532 0x63206e6f
3fc9eeb0: 0x2065726f 0x00000030 0x40380000 0xd3aec498 0x000001d0 0x3fcd9bc8 0x3fcd85c0 0x420402e6
3fc9eed0: 0x3fcd1cd4 0x3fcd1cd4 0x3fcd85c0 0x3fc961b4 0x3fcd1cbc 0x3fcd9bc8 0x3fcd85c0 0x4204034a
3fc9eef0: 0x3fcd85c0 0x3fcd9bc8 0x3fcd85c0 0x420402d6 0x3fcd1cbc 0x3fcd9bc8 0x00000004 0x42002578
3fc9ef10: 0x00000065 0x00000064 0x00000000 0x3fc9ef64 0x3fcb2da4 0x00000001 0x3fcb2da4 0xaaaaaaab
3fc9ef30: 0x0000015f 0x00000064 0x00000000 0x000001c5 0x3fcb71d0 0x00000068 0x3fc9f004 0x4202144c
3fc9ef50: 0x3fc9ef94 0x3fcb71ec 0x3fc96000 0x00000068 0x00000100 0x50000f1c 0x500010dc 0x500010dc
3fc9ef70: 0x00000000 0x00000000 0x00000000 0x3fc9ef84 0x0000000a 0x706d6f63 0x6e656e6f 0x00007374
3fc9ef90: 0x3fc9ef80 0x3fc9ef9c 0x00000001 0x00000020 0x20736920 0x20656874 0x00757274 0xd3aec498
3fc9efb0: 0x00000161 0x3fc96c20 0x00001800 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9efd0: 0x00000000 0x00000000 0x0000000d 0x0000000a 0x3fc996dc 0x3fc96000 0x3fc9f064 0x42001da4
3fc9eff0: 0x3fc996dc 0x50001b3c 0x3fcb71d0 0x3fcb71ec 0x3fcb71ec 0x3fcd1cbc 0x3fcd1cd4 0x3fcd1cd4
3fc9f010: 0x00000000 0x00000000 0x0000000d 0x00000160 0x3fc996dc 0x50001b3c 0x3fccfe98 0x0000000a
3fc9f030: 0x3fc996dc 0x3fc96000 0x3fc9969c 0x420028fa 0xffffffff 0x3fcd0b74 0x3fccfe98 0x3fcb2da4
3fc9f050: 0x000001d0 0x00000006 0x3fc99278 0x420247fe 0x00000000 0x3fc9969c 0x3fc9f054 0x3fc9f04c
3fc9f070: 0x3fc9f050 0x50001cb0 0x0000015f 0x00000160 0x500016f4 0x500016a4 0x500018a4 0xd3aec498
3fc9f090: 0x500016f4 0x00000000 0x00000000 0x00000000 0x00000000 0x3fc96000 0x3fc96000 0x42004ec2
3fc9f0b0: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9f0d0: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9f0f0: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x3fc9f114
3fc9f110: 0x00000006 0x6c626174 0x00002e65 0x00000000 0x00000000 0x00000000 0x00000000 0x012c0802
3fc9f130: 0x65011400 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9f150: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9f170: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9f190: 0x4038b338 0x40383a64 0x3fc9f210 0x3fc8ea00 0x3fa15cd8 0x00800000 0xecc00000 0x01000000
3fc9f1b0: 0x40053aec 0x4202480c 0x3fc9f230 0x3fc8ea00 0x3fa15cd8 0x00800000 0x0ef19b46 0x01000000
3fc9f1d0: 0x00000000 0x3fc96000 0x0ef19b46 0x00000000 0x000003e8 0x00000008 0x3ff19e18 0x00000000
3fc9f1f0: 0x000003e8 0x00000003 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000
3fc9f210: 0x00000000 0x00000000 0x00000000 0x00000000 0x00000040 0x00000040 0x00000010 0x420247fe
3fc9f230: 0x00000000 0x00000000 0x00000000 0xd3aec498 0x00000040 0x00000040 0x00000000 0x42026530



ELF file SHA256: 8d52a9d33fee5231

Rebooting...
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x3 (RTC_SW_SYS_RST),boot:0xf (SPI_FAST_FLASH_BOOT)
Saved PC:0x40382202
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5810,len:0x38c
load:0x403cc710,len:0x6a0
load:0x403ce710,len:0x2624
entry 0x403cc710
␛[2J␛[H=== eenk Interactive Fiction Runtime (ESP32-C3) ===
Free heap before init: 272504 bytes
[BootManager] Initialised (using NVS)
[Boot] mode=1
[Boot] INK_RUNTIME — loading story...
```

</details>

* Investigate why stories take a long time to load from boot when using SD font
<details>

<summary>Boot logs (from power on)</summary>

```
[Boot] INK_RUNTIME — loading story...
[SD] SD card detected
[SD] SD card mounted OK
[SD] Found story: /stories/the_intercept/main.bin
[FlashCache] ink_cache partition found, streaming to flash...
[FlashCache] Loading: 0%
[FlashCache] Loading: 100%
[InkEngine] Story loaded from memory — 151024 bytes
[  3818][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-regular.epdfont does not exist, no permits for creation
[  3883][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-regular.epdfont does not exist, no permits for creation
[  3948][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-regular.epdfont does not exist, no permits for creation
[  3963][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Regular.epdfont does not exist, no permits for creation
[  4028][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Regular.epdfont does not exist, no permits for creation
[  4093][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Regular.epdfont does not exist, no permits for creation
[  4108][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small.epdfont does not exist, no permits for creation
[  4172][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small.epdfont does not exist, no permits for creation
[  4236][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small.epdfont does not exist, no permits for creation
[  4250][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small.epdfont does not exist, no permits for creation
[  4314][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small.epdfont does not exist, no permits for creation
[  4378][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small.epdfont does not exist, no permits for creation
[  4392][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-regular.epdfont does not exist, no permits for creation
[  4457][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-regular.epdfont does not exist, no permits for creation
[  4522][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-regular.epdfont does not exist, no permits for creation
[  4539][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-regular.epdfont does not exist, no permits for creation
[  4605][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-regular.epdfont does not exist, no permits for creation
[  4671][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-regular.epdfont does not exist, no permits for creation
[  4687][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Regular.epdfont does not exist, no permits for creation
[  4753][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Regular.epdfont does not exist, no permits for creation
[  4819][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Regular.epdfont does not exist, no permits for creation
[  4835][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small.epdfont does not exist, no permits for creation
[  4901][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small.epdfont does not exist, no permits for creation
[  4966][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small.epdfont does not exist, no permits for creation
[  4982][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small.epdfont does not exist, no permits for creation
[  5047][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small.epdfont does not exist, no permits for creation
[  5112][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small.epdfont does not exist, no permits for creation
[  5128][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-regular.epdfont does not exist, no permits for creation
[  5194][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-regular.epdfont does not exist, no permits for creation
[  5260][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-regular.epdfont does not exist, no permits for creation
[StreamingEpdFontFamily] Loaded: /fonts/serif-small-regular.epdfont
[  5305][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold.epdfont does not exist, no permits for creation
[  5371][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold.epdfont does not exist, no permits for creation
[  5435][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold.epdfont does not exist, no permits for creation
[  5450][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Bold.epdfont does not exist, no permits for creation
[  5514][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Bold.epdfont does not exist, no permits for creation
[  5578][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Bold.epdfont does not exist, no permits for creation
[  5593][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold.epdfont does not exist, no permits for creation
[  5657][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold.epdfont does not exist, no permits for creation
[  5721][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold.epdfont does not exist, no permits for creation
[  5738][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold.epdfont does not exist, no permits for creation
[  5803][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold.epdfont does not exist, no permits for creation
[  5868][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold.epdfont does not exist, no permits for creation
[  5884][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Bold.epdfont does not exist, no permits for creation
[  5950][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Bold.epdfont does not exist, no permits for creation
[  6015][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Bold.epdfont does not exist, no permits for creation
[  6031][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold.epdfont does not exist, no permits for creation
[  6097][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold.epdfont does not exist, no permits for creation
[  6162][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold.epdfont does not exist, no permits for creation
[StreamingEpdFontFamily] Loaded: /fonts/serif-small-bold.epdfont
[  6207][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-italic.epdfont does not exist, no permits for creation
[  6273][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-italic.epdfont does not exist, no permits for creation
[  6337][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-italic.epdfont does not exist, no permits for creation
[  6352][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Italic.epdfont does not exist, no permits for creation
[  6417][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Italic.epdfont does not exist, no permits for creation
[  6481][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-Italic.epdfont does not exist, no permits for creation
[  6496][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-italic.epdfont does not exist, no permits for creation
[  6561][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-italic.epdfont does not exist, no permits for creation
[  6625][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-italic.epdfont does not exist, no permits for creation
[  6642][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-italic.epdfont does not exist, no permits for creation
[  6708][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-italic.epdfont does not exist, no permits for creation
[  6774][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-italic.epdfont does not exist, no permits for creation
[  6790][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Italic.epdfont does not exist, no permits for creation
[  6856][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Italic.epdfont does not exist, no permits for creation
[  6922][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-Italic.epdfont does not exist, no permits for creation
[  6938][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-italic.epdfont does not exist, no permits for creation
[  7004][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-italic.epdfont does not exist, no permits for creation
[  7070][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-italic.epdfont does not exist, no permits for creation
[StreamingEpdFontFamily] Loaded: /fonts/serif-small-italic.epdfont
[  7115][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  7182][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  7247][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  7262][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  7327][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  7392][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  7407][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  7472][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  7537][E][vfs_api.cpp:105] open(): /sd/stories/the_intercept/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  7554][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  7621][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  7687][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  7704][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  7770][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  7836][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  7853][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  7919][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  7985][E][vfs_api.cpp:105] open(): /sd/stories/main/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  8002][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  8067][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  8132][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-bolditalic.epdfont does not exist, no permits for creation
[  8148][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  8214][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  8279][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-BoldItalic.epdfont does not exist, no permits for creation
[  8295][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  8361][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-bold-italic.epdfont does not exist, no permits for creation
[  8427][E][vfs_api.cpp:105] open(): /sd/fonts/serif-small-bold-italic.epdfont does not exist, no permits for creation
[StreamingEpdFontFamily] Loaded family 'serif-small' (base 'serif-small'): B=1 I=1 BI=0
[InkEngine] Font: SD family 'serif-small'
Save loaded successfully!
Free heap after load: 93672 bytes
```
</details>

* Battery charge indicator never shows up in the battery widget. **Root cause identified:** the X4 uses `BatteryMonitor(GPIO0)` (ADC mode), and `BatteryMonitor::isCharging()` always returns `false` in ADC mode — only the BQ27220 fuel gauge (X3 hardware) supports charge detection. The charge LED on the X4 is driven directly by the charge IC hardware, with no GPIO routed to the ESP32-C3. Options: (a) leave as-is and remove the charging label path from BatteryWidget for cleanliness, (b) check if the X4 schematic exposes a charge-detect GPIO that we missed, or (c) detect charger presence via USB VBUS detection if a suitable pin is available.

## UI & UX

* Fix the Settings view input methods. left right should be up down like the side buttons, regardless of the item. Back should always exit settings and the action button should cycle through option values for the selected setting item. This way we can use the side buttons (up / down / power as action) or the bottom buttons indifferently. 
* Implement drop shadow and offset for the selected item in list view like Settings and Library. Maybe these could be factored in a common UI component if it's flexible enough in terms of input options and layout inside the items.
* The OTA updater mode needs some love, text doesn't even wrap.

## Refactoring

* Unit tests for Core, Scripting, SystemUI, etc. (some exist, need more)
* Evaluate if we can remove global state (battery, input, display) - pass them into SystemUI (and the App?) at construction time. SystemUI already accepts IInput so that's already being done.
* Split SystemUI into smaller widgets.
* Optimize system fonts (we can drop most of the character table for the all caps FONT_HEADING font).
* Do we need to keep ArabicShaper and ThaiShaper? 

## New features
* Image support in stories
  - pack images in a sidecar binary and update the main story header with a flag to indicate the sidecar is required.
  - uring compilation we should convert the images to appropriate optimized format (look how papyrix did it). enforce max sizes including for the optional thumbnail and cover.
  - implement a generic ImageWidget that takes an image index, size, and scaling mode, draws it in the current EPD page using appropriate scaling (fit, fill, or crop) and hal-specific image drawing primitives, and supports optional grayscale transformation.
  - update StoryRenderer to parse @image tags, cache image indices in a sidecar, and delegate drawing to ImageWidget.
  - add image_count and page_size fields to StoryHeader, and a --pack-images option to eenky to generate sidecars and update headers.
  - add an optional image cover that can be set as part of the story metadata header in ink (with a new @cover tag and @thumbnail tags).
  - update the documentation, and update eenky to support this in the web preview as well.
  - the thumbnail image should be displayed in the library view next to the story title. 
  - the cover image should be used for the sleep screen.
  - add visual tests for these.
* EPUB support
  - Adding basic support for epub files and rendering them as different items in the library (or a separate section) would be nice. We don't need this to be super advanced we just need to support rendering and saving progress.
  - Hopefully we can plug papyrix logic into our renderer.