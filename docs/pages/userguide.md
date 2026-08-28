<div class="page-content">

# eenk User Guide

Welcome to the **eenk** User Guide! This guide covers everything you need to know about using your Xteink e-ink device running the eenk interactive fiction firmware.

## Overview & Supported Hardware

**eenk** transforms your pocket e-reader into a dedicated, distraction-free Interactive Fiction player for stories written in [inkle's Ink](https://www.inklestudios.com/ink/) narrative language.

### Supported Devices

> Important: Only USB-unlocked Xteink devices are compatible with the eenk firmware.

| Device | Processor | Notes |
|---|---|---|
| **Xteink X4** | ESP32-C3 | Reference platform |
| **Xteink X4 Pro** | ESP32-S3 | Touch and backlight are supported; extra PSRAM is used for faster story loading |
| **Xteink X3** | ESP32-C3 | Same chip as X4 with different screen resolution, now also supported |


## Flashing eenk

### Method 1: Web Flasher (Recommended)

The easiest way to install or update eenk firmware is using your web browser:

1. Connect your device via USB-C.
2. Open the [Flasher](../flasher/) in a browser that [supports Web serial](https://caniuse.com/wf-serial) such as Chrome.
    > Note: you can also use it via the Device Manager window of the [eenky IDE](../eenky) if you have it installed.
3. Select your device model and click **Connect**.
4. Follow the on-screen wizard to flash the firmware. Select **Factory flash** if it's the first time you're flashing eenk, **Update** otherwise.

### Method 2: Offline SD Card Update

TODO: document this alternative method for X4.

<!-- 1. Download the [latest `firmware.bin` release](https://github.com/t0mg/eenk/releases/latest) for your device (`x3x4-` or `x4pro-` depending on your model).
2. Copy `firmware.bin` to the **root directory** of your MicroSD card.
3. Insert the SD card and turn on the device. The bootloader will detect the update file, flash the new firmware, and rename the file to `firmware.bin.bak`. -->

## Basic Device Interactions

### Power & Sleep / Wake

- **Power On / Wake Up**: Press and hold the **Power** button for ~2–3 seconds until the display refreshes.
- **Power Off / Sleep**: Press and hold the **Power** button for ~2–3 seconds. The device will save your current progress and show the sleep cover before cutting power.
- **Auto-Sleep**: By default, the device will automatically save and sleep after 2 minutes of inactivity (configurable in Settings).

> Note: When connected via **USB**, the microcontroller remains powered by the USB host. To test true deep sleep and battery longevity, disconnect the USB cable.

### Hardware Controls & Navigation

The Xteink X3 and X4 have 4 chin buttons that we call (from left to right) **BACK**, **CONFIRM**, **LEFT**, **RIGHT**. Additionally there are 3 buttons on the right side of the display, from top to bottom: **POWER**, **UP**, **DOWN**.

The Xteink X4 Pro has 3 physical buttons on the sides: **LEFT** on one side, and **RIGHT** and **POWER** on the other. A single additional capacitive button (**MENU**) is located at the center of the chin below the display.

Buttons are used as follows in eenk:

| Control | In Story Player | In EPUB Reader | In Library / Menus | In Settings View |
|---|---|---|---|---|
| **UP / LEFT** | Scroll text up / Previous choice | Previous page | Move selection up | Move selection up |
| **DOWN / RIGHT** | Scroll text down / Next choice | Next page | Move selection down | Move selection down |
| **BACK / QUIT / MENU** | Open exit dialog (save & pause) | Open exit dialog (save & pause) | Open Settings panel | Exit Settings (saves automatically) |
| **CONFIRM / SELECT / POWER (Short press)** | Confirm selected choice | Next page | Launch selected story | Cycle value / Run action |
| **POWER (Long Press)** | Save progress and sleep | Bookmark page and sleep | Sleep | Save settings and sleep |

Generally speaking when there are modal windows or a footer with labels, the corresponding chin button should be used.

In the first example below, the leftmost chin button (BACK) is used to close the modal window, and the rightmost (RIGHT) is used for the Confirm action (the 2 central chin buttons are not used in this case).

![Footer buttons](../assets/images/chin.png)

In the second example above, BACK button is for Back, CONFIRM button is for Change, LEFT button for Prev and RIGHT button for Next.

> Note: In eenky's simulator, arrow keys are used for UP/DOWN/LEFT/RIGHT and the Enter key for CONFIRM.

### Touchscreen Controls (Xteink X4 Pro)

The X4 Pro features a capacitive touchscreen with full touch gestures offering alternative controls:

- **Scroll Narrative**: Swipe **UP** or **DOWN** anywhere on the screen to scroll through previous text.
- **Select Choices**: Tap directly on any choice button on the screen to choose it.
- **Quick Settings Menu**: Swiping **down from the top edge** of the screen opens the Quick Settings panel (see below).

### Quick Settings Menu (X4 Pro)

On the X4 Pro, swipe down from the top edge to access quick controls:

- **Brightness**: Adjust backlight display brightness.
- **Warmth**: Adjust backlight color warmth.
- **Toggles**: Toggle backlight on/off and enable/disable touch controls for scrolling and story choices.
- **Sleep Device**: Put the device to sleep instantly.

## Getting & Transferring Stories

Stories in eenk are distributed as **`.eenk` story packages** compiled with the companion [**eenky IDE**](../eenky/) or downloaded from the [**Stories Catalog**](../stories/) and author websites. A `.eenk` file is a self-contained archive (ZIP format under the hood) that bundles everything needed to run your story.

### Transfer via USB Device Manager (Recommended)

The simplest and fastest way to install stories is over USB without removing the MicroSD card:

1. Turn on your eenk device and remain on the **Library** screen.
2. Connect your device to your computer via USB-C.
3. Open the **Device Manager** in the [eenky IDE](../eenky/) or open the [Web Device Manager](../device-manager/) in a browser that supports Web Serial (such as Google Chrome or Microsoft Edge).
4. Click **Connect** and select your device port (typically named `USB JTAG/serial debug unit` or `USB Serial`)
5. Drag your `.eenk` file onto the upload area. The Device Manager automatically inspects the package and uploads the story into a dedicated folder on your device.
6. When the transfer finishes, click **Disconnect**. The device will refresh its library and your new story will be ready to play immediately.

> **Tip:** You can also install curated stories with a single click directly from the [Stories Catalog](../stories/)!

### Manual Transfer via MicroSD Card

> **Warning:** Direct MicroSD card transfer is **not recommended** because `.eenk` packages cannot be read by the firmware as a single file on the SD card. You must manually extract the package contents first.

If you cannot connect your device via USB, you can unpack and copy stories manually:

1. Turn off your device, remove the MicroSD card, and connect it to your computer using an SD card reader.
2. Rename your story package file from `storyname.eenk` to `storyname.zip`.
3. Extract the ZIP archive on your computer. You will find `story.bin` and any accompanying `.media` or `.epdfont` files.
4. On the MicroSD card, navigate to the `/stories/` folder (create it if it doesn't exist).
5. Create a **new subfolder** named after your story (e.g. `/stories/my_adventure/`).
6. Copy the extracted `story.bin` and sidecar files into that subfolder:

```text
MicroSD Root
├── stories/
│   ├── the_intercept/
│   │   ├── story.bin
│   │   ├── story.media
│   │   └── SpecialElite.epdfont
│   └── my_adventure/
│       ├── story.bin
│       └── story.media
├── books/
│   └── frankenstein.epub
├── fonts/
│   ├── Literata.epdfont
│   └── Literata-bold.epdfont
└── .eenk_saves/
    └── the_intercept.sav
```

7. Eject the MicroSD card safely, insert it back into your device, and power on.

## Playing Stories on eenk

### The Library Browser

When you power on the device, the **Library** displays all available stories found on your SD card:

- Each entry displays the story's title, author, file size, and thumbnail preview.
- Stories that are currently loaded or have existing save states display a badge indicator.
- Use **UP / DOWN** to navigate and **CONFIRM** or a short press on **POWER** to launch a story.

### Loading & Memory Architecture

The loading process differs depending on your hardware model:

- **Xteink X4 / X3 (ESP32-C3)**:
  Because the ESP32-C3 has less internal RAM than an original [Commodore Amiga 500](https://en.wikipedia.org/wiki/Amiga_500), eenk uses an **internal flash cache partition** (`ink_cache`). When you select a new story, eenk streams the story from the SD card into this flash cache. You will see a `Loading story...` progress screen for a few seconds. Once cached, story execution runs via high-speed, zero-copy memory-mapped flash. This means that resuming a story you've been playing is relatively fast because it was already loaded in cache, but switching between stories takes time as this cache needs to be rewritten every time.
- **Xteink X4 Pro (ESP32-S3)**:
  With 8MB of additional RAM, the X4 Pro loads stories directly into memory every time it loads, eliminating the need for a cache partition.
  
  > Note: A technical side effect is that the partition table on the X4Pro isn't too different from the e.g. Crosspoint firmware, unlike on X3/X4 where it's OTA partitions are assymetrical.

### Reading, Scrolling & Making Choices

- **Reading**: Story text is automatically formatted, hyphenated, and paginated for the e-ink screen.
- **Scrolling**: Use **UP / DOWN** or touch swipe to scroll through previous story paragraphs.
- **Choices**: At the end of each turn's text output, a separator line and downward indicator (`▼`) indicate that choices are available:
  - Scroll down (via **DOWN / RIGHT**, **SWIPE UP**, or **POWER** button click) to reveal all available choices.
  - Cycle through options with **UP / DOWN** (or tap directly on touch screens).
  - Selected choices display focused highlighting.
  - Press **CONFIRM** or short press **POWER** to make your choice and advance the story.

### Automatic Save States & Resuming

eenk automatically saves your full story state whenever you exit or put the device to sleep:

- Save files are stored on the SD card in `/.eenk_saves/<story_name>.sav`.
- The save file captures the complete Ink virtual machine state (variables, callstack, visited knots, and full reading history).
- When you launch a previously played story from the library, eenk will automatically resume right where you left off.

### Exiting or Restarting a Story

- **Exit during play**: Press **BACK / QUIT** at any time. A confirmation dialog will appear: choose **Confirm** to save and return to the library, or **Cancel** to continue reading.
- **End of Story**: When you reach the end of a story, eenk presents two options:
  - **Exit Story**: Save and return to the library.
  - **Restart Story**: Clear the save file and begin again from the beginning.

## System Settings

Access the **Settings View** by pressing **BACK** or **MENU** from the Library screen.

This UI screen is still a work in progress and likely to be adjusted in the near future.

<!-- | Setting | Options | Description |
|---|---|---|
| **Story Font** | Default / SD Fonts | Select your preferred font family for story text. |
| **Choice Font** | Small, Medium, Large | Adjust the font size used for choice buttons. |
| **Margin** | 8px (Tight), 16px (Normal), 24px (Wide), 32px (Extra Wide) | Adjust horizontal reading margins. |
| **Full Refresh** | Every 5, 10, 15, 20 updates, or Off | Periodically trigger a full e-ink waveform refresh to eliminate ghosting. |
| **Sleep & Save** | 1 min, 2 min, 5 min, Never | Set inactivity timeout before auto-saving and deep sleeping. |
| **Override Story Font** | On / Off | If enabled, your device font setting will override any `@font` hint defined by the story. |
| **Reboot to Updater** | OTA / App1 | Reboot into the recovery/updater partition (X4). |
| **Format SD** | Erase SD Card | Formats the MicroSD card to FAT32 filesystem. |
| **Firmware** | Version String | Displays the currently installed eenk firmware version. | -->

## EPUB reading

eenk also includes a basic EPUB reader so you can take the occasional break between your adventures and read some traditional books.

EPUB files can be uploaded via the Device Manager or copied directly to the `/books` folder on your MicroSD card. Books will appear in your Library alongside your interactive fiction stories.

> **Important Note on Large EPUB Files (> 2 MB):**
> Due to USB serial bandwidth limitations and hardware transfer timing constraints, transferring EPUB files larger than 2 MB over USB serial may be very slow or fail with transfer errors.
> For books larger than 2 MB (especially EPUBs containing illustrations or complex layouts), **copying files directly to the `/books` folder on your MicroSD card via an SD card reader is strongly recommended**.

A bookmark is stored in `.eenk_saves/` for each book you open. The `.eenk_cache` folder contains processed content such as images for faster loading between readings. This cache is cleaned up when you delete the book from the device manager.

## Font Management

eenk features a versatile typography engine adapted from the [Papyrix](https://github.com/bigbag/papyrix-reader) firmware.

### Built-in Fonts

The firmware includes embedded bitmap fonts ready to use out of the box:
- `sans` / `sans-medium`: Clean 16pt sans-serif font (Open Sans).
- `sans-small`: Compact 14pt sans-serif font.
- `serif` / `serif-medium`: Elegant 16pt serif font (Literata).
- `serif-large`: Larger serif font for comfortable reading.

### Adding Custom SD Fonts

You can add any custom font by placing `.epdfont` files created with the eenky IDE into the `/fonts/` directory on your SD card, or inside a story's folder:

- **Font Family Naming**: To support bold and italic formatting, provide matching style suffixes:
  - `myfont.epdfont` (Regular)
  - `myfont-bold.epdfont` (Bold)
  - `myfont-italic.epdfont` (Italic)
  - `myfont-bolditalic.epdfont` (Bold Italic)
- **Synthetic Fallbacks**: If your custom font does not include separate bold or italic files, eenk automatically generates synthetic bold and oblique styles on the fly!
- **Converting Fonts**: You can convert standard TrueType (`.ttf`) or OpenType (`.otf`) fonts to `.epdfont` format using the [eenky IDE](../eenky/).

## Recovery & Troubleshooting

If you ever encounter an issue or a corrupted firmware flash, eenk provides built-in recovery mechanisms:

### Hardware Recovery Modes

- **Xteink X4 Recovery**:
  Turn on the device while holding the **UP** button. This forces the device to boot into the minimal recovery Updater partition (`app1`).
- **Xteink X4 Pro Recovery**: TODO: verify this
  <!-- Turn on the device while holding the **LEFT** button (GPIO0). -->
- **USB Bootloader Reflash**:
  If the device does not turn on normally, hold the **UP/BOOT** button while plugging in the USB cable to enter ESP32 ROM bootloader mode, then reflash using the [Web Flasher](../flasher/).

### Frequently Asked Questions

TODO !

<!-- 
<details>
<summary><strong>My SD card is not recognized or stories don't appear.</strong></summary>
<p>Ensure your MicroSD card is formatted as <strong>FAT32</strong> (or exFAT on supported builds) and that story files have the <code>.bin</code> extension inside the <code>/eenk/</code> directory.</p>
</details>

<details>
<summary><strong>Why does loading a story take a few seconds on the X4?</strong></summary>
<p>The X4 copies the story from SD card to internal flash cache on the first launch so it can execute smoothly without stuttering. Resuming the same story afterwards is instantaneous.</p>
</details>

<details>
<summary><strong>How do I clean up e-ink ghosting?</strong></summary>
<p>Open Settings and adjust <strong>Full Refresh</strong> to <code>Every 5 updates</code> or <code>Every 10 updates</code>. This periodically triggers a complete screen refresh to clear residual ghost images.</p>
</details> -->

</div>
