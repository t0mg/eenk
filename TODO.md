# Backlog for eenk.

* See tools/eenky/TODO.md for the eenky TODOs.

## Bugs

* eenky frontend font decode error: `OTS parsing error: invalid sfntVersion: 1008821359` for Literata-VariableFont.
* Battery charge indicator never shows up in the battery widget. **Root cause identified:** the X4 uses `BatteryMonitor(GPIO0)` (ADC mode), and `BatteryMonitor::isCharging()` always returns `false` in ADC mode — only the BQ27220 fuel gauge (X3 hardware) supports charge detection. The charge LED on the X4 is driven directly by the charge IC hardware, with no GPIO routed to the ESP32-C3. Options: (a) leave as-is and remove the charging label path from BatteryWidget for cleanliness, (b) check if the X4 schematic exposes a charge-detect GPIO that we missed, or (c) detect charger presence via USB VBUS detection if a suitable pin is available.

## UI & UX

* The OTA updater mode needs some love, text now wraps but the ui is unstyled.

## Refactoring

* Add unit tests and increase coverage.
* Evaluate if we can remove global state (battery, input, display) - pass them into SystemUI (and the App?) at construction time. SystemUI already accepts IInput so that's already being done.
* Split SystemUI into smaller widgets.

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