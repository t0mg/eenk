# Backlog for eenk.

* See tools/eenky/TODO.md for the eenky TODOs.

## Bugs

* [x] in the story player, swipe to scroll doesn't really work. while scrolling a simple tap on the screen scrolls all the way down. (Resolved: tracked swipe end coordinates, fixed tap early-return bug, added gated TOUCH_DEBUG logs)
* [x] battery measurement doesn't work on either X4 and X4Pro. (Resolved: fixed X4 ADC pin to GPIO 0, added CW2017 BATINFO initialization & I2C teardowns for X4 Pro)

## UI & UX

* The OTA updater mode needs some love, text now wraps but the ui is unstyled.

## Refactoring

* Add unit tests and increase coverage.
* Evaluate if we can remove global state (battery, input, display) - pass them into SystemUI (and the App?) at construction time. SystemUI already accepts IInput so that's already being done.

## New features
* X4Pro support
  - Touch support (tap footer and modal labels, swipe to scroll)
  - LED support (brightness + color temperature)
  - Notification style pull down menu for quick settings (for LED)
* EPUB support
  - Adding basic support for epub files and rendering them as different items in the library (or a separate section) would be nice. We don't need this to be super advanced we just need to support rendering and saving progress.
  - Hopefully we can plug papyrix logic into our renderer.