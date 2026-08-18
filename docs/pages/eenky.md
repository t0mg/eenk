<div class="page-content">

# Eenky IDE

**eenky** is the desktop companion application for writing, compiling, and deploying Ink interactive fiction stories to your eenk device. It is forked from [inkle's Inky](https://github.com/inkle/inky) but uses a new code editor and is extended with the eenk compilation pipeline, a pixel-accurate device simulator, and a USB Device Manager.

## Installation

Download the latest eenky release for your platform from the [GitHub Releases](https://github.com/t0mg/eenky/releases) page, or build from source by following the [build instructions](../build/).

### Security Warnings Troubleshooting

This app is free, open-source software. Because official developer signing certificates carry significant annual fees, the current binaries are **not officially code-signed**. 

As a result, your operating system might flag eenky as untrusted on first launch. Follow these quick steps to mute the false alarm:

#### On macOS
<details>
<summary>"App is damaged and cannot be opened"</summary>

Modern macOS Gatekeeper quarantines unnotarized binaries downloaded from the internet. To unblock it:
1. Move the app to your `/Applications` folder.
2. Open **Terminal** and run:
```bash
xattr -cr /Applications/eenky.app
```
3. Launch the app normally.
</details>

#### On Windows

<details>
<summary>"Windows protected your PC" / SmartScreen</summary>

Microsoft Defender SmartScreen flags downloads from unverified publishers:
1. When the blue banner appears, click **"More info"**.
2. Click the **"Run anyway"** button at the bottom.
</details>

## Features

- **CodeMirror 6** editor with Ink syntax highlighting
- **Made for Ink** - most of the niceties from Inky have been recreated, such as the live preview, universal search, knot browser, include management, live syntax parsing, snippets, bult-in ink documentation, web export, etc.
- **One-click compile** - runs the custom `inklecate → inkcpp_cl → eenk` pipeline to produce the `.bin` file the firmware expects, automatically converts `.ttf` files to the `.epdfont` format used by eenk, and dithers and packs images into a `.media` "sidecar" file
- **Built-in simulator** - test your story and preview what it will look like on a simulated, pixel-perfect 800×480 e-ink display
- **Device Manager** - easily transfer stories and save files over USB, without ever pulling the SD card out (also available [on this website](/device-manager)).
- **Flash Firmware** - wizard-style flasher for installing and updating the firmware (also available [on this website](/flasher)).
- **Dark Theme** - turn off the lights in one click.

</div>
