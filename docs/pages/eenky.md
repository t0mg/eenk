<div class="page-content">

# Eenky IDE

**eenky** is the desktop companion application for writing, compiling, and deploying Ink interactive fiction stories to your eenk device. It is forked from [inkle's Inky](https://github.com/inkle/inky) but uses a new code editor and is extended with the eenk compilation pipeline, a pixel-accurate device simulator, and a USB Device Manager.

> Note: Only the Windows build is available at the moment, Linux and MacOS will come when/if I can find a development machine for them.

## Installation

Download the latest eenky release for your platform from the [GitHub Releases](https://github.com/t0mg/eenky/releases) page, or build from source by following the [build instructions](/build/).

## Features

- **CodeMirror 6** editor with Ink syntax highlighting
- **Made for Ink** - most of the niceties from Inky have been recreated, such as the live preview, universal search, knot browser, include management, live syntax parsing, snippets, bult-in ink documentation, web export, etc.
- **One-click compile** - runs the custom `inklecate → inkcpp_cl → eenk` pipeline to produce the `.bin` file the firmware expects, automatically converts `.ttf` files to the `.epdfont` format used by eenk, and dithers and packs images into a `.media` "sidecar" file
- **Built-in simulator** - test your story and preview what it will look like on a simulated, pixel-perfect 800×480 e-ink display
- **Device Manager** - easily transfer stories and save files over USB, without ever pulling the SD card out (also available [on this website](/device-manager)).
- **Flash Firmware** - wizard-style flasher for installing and updating the firmware (also available [on this website](/flasher)).
- **Dark Theme** - turn off the lights in one click.

</div>
