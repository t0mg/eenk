# eenk Firmware Credits

The **eenk** core firmware runtime and hardware ecosystem are made possible by the following open-source projects, libraries, and creators:

## Core

- **[Papyrix](https://github.com/bigbag/papyrix-reader)** - Developed by [Pavel Liashkov](https://github.com/bigbag) is the lightweight custom Xteink firmware that served as the basis for eenk, providing the core text rendering engine (`GfxRenderer`), text layout system, bitmap font format (`EpdFont`), and a lot of inspiration :)
- **[inkcpp](https://github.com/JBenda/inkcpp)** - Developed by [JBenda](https://github.com/JBenda). Low-memory C++ runtime engine for executing compiled Ink story binaries. It's the magic that makes it all possible. Used via the [eenk-patches fork](https://github.com/t0mg/inkcpp).

## Other building blocks

- **[Free-Ink SDK](https://github.com/Free-Ink/freeink-sdk)** - Created by the Free-Ink team. Hardware abstraction SDK providing low-level display drivers (UC8253, UC8279, SSD1677, UC8179), power control, battery monitoring, and device detection for Xteink hardware. Additionally the [FreeInkBook](https://freeink.org/docs/lib-book) reading engine is used for the EPUB reader functionality. 
- **[esptool-js](https://github.com/espressif/esptool-js)** - Espressif Systems. Web Serial JavaScript library enabling browser-based firmware flashing.
- **[Arduino-ESP32 / ESP-IDF](https://github.com/espressif/arduino-esp32)** - Espressif Systems. Core ESP32 hardware platform, peripheral drivers, and framework.
- **[PlatformIO](https://platformio.org/)** - PlatformIO team. Build automation system for embedded hardware and host compilation.
- **[SDL2](https://www.libsdl.org/)** - Sam Lantinga & Simple DirectMedia Layer team. Cross-platform graphics library powering eenky's native desktop simulator.
- **[Antigravity](https://antigravity.google/)** - Google. A coding assitant that allowed me to bring this small "what if" project to life relatively quickly. It also enabled a lot of feature creep.

## Fonts

- **[Inter](https://rsms.me/inter/)** - Designed by Rasmus Andersson.
- **[Syne](https://github.com/BonjourMonde/Syne-Typeface)** - Designed by Bonjour Monde and Lucas Descroix.
- **[Literata](https://fonts.google.com/specimen/Literata)** - Designed by TypeTogether.
- **[Material Symbols Outlined](https://fonts.google.com/icons)** - Google. Icon font used across the documentation site, flasher, and device manager.
- **Extra EpdFonts** from the [Papyrix project](https://github.com/bigbag/papyrix-reader/tree/master/lib/EpdFont/src/builtinFonts).