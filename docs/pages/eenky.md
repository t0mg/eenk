<div class="page-content">

# Eenky IDE

**eenky** is the desktop companion application for writing, compiling, and deploying Ink interactive fiction stories to your eenk device. It is an [Electron](https://www.electronjs.org/) + [Vue 3](https://vuejs.org/) application forked from [inkle's Inky](https://github.com/inkle/inky), extended with the eenk compilation pipeline, a pixel-accurate device simulator, and a USB Device Manager.

## Features

- **CodeMirror 6** editor with Ink syntax highlighting
- **Made for Ink** - most of the niceties from Inky have been recreated, such as the live preview, universal search, knot browser, include management, live syntax parsing, snippets, bult-in ink documentation, web export, etc.
- **One-click compile** - runs the custom `inklecate → inkcpp_cl → eenk` pipeline to produce the `.bin` file the firmware expects, automatically converts `.ttf` files to the `.epdfont` format used by eenk, and dithers and packs images into a `.media` "sidecar" file
- **Built-in simulator** - test your story and preview what it will look like on a simulated, pixel-perfect 800×480 e-ink display
- **Device Manager** - easily transfer stories and save files over USB, without ever pulling the SD card out (also available [on this website](/device-manager)).
- **Flash Firmware** - wizard-style flasher for installing and updating the firmware (also available [on this website](/flasher)).
- **Dark Theme** - turn off the lights in one click.

## Installation

Download the latest eenky release for your platform from the [GitHub Releases](https://github.com/t0mg/eenky/releases) page, or build from source:

```bash
npm install
npm run setup    # Builds simulator + compiler binaries
npm start        # Launch in dev mode
```

</div>
