<div class="page-content">

# Eenky IDE

**eenky** is the desktop companion application for writing, compiling, and deploying Ink interactive fiction stories to your eenk device. It is an [Electron](https://www.electronjs.org/) + [Vue 3](https://vuejs.org/) application forked from [inkle's Inky](https://github.com/inkle/inky), extended with the eenk compilation pipeline, a pixel-accurate device simulator, and a USB Device Manager.

## Features

- **CodeMirror 6** editor with Ink syntax highlighting and snippet support
- **One-click compile** — runs `inklecate → inkcpp_cl` to produce a `.bin` ready for hardware
- **Integrated simulator** — renders your story in a live 800×480 e-ink window
- **Device Manager** — browse, upload, and delete stories on your eenk device via USB Serial
- **Flash Firmware** — wizard-style Web Serial flasher for installing and updating firmware
- **Custom font conversion** — converts `.ttf` files to the `.epdfont` format used by eenk
- **Image processing** — dithers and packs images into the `.media` sidecar format

## Installation

Download the latest eenky release for your platform from the [GitHub Releases](https://github.com/t0mg/eenky/releases) page, or build from source:

```bash
cd tools/eenky/app
npm install
npm run setup    # Builds simulator + compiler binaries
npm start        # Launch in dev mode
```

## Keyboard Shortcuts

| Action | Windows / Linux | macOS |
|--------|----------------|-------|
| Compile | `Ctrl+B` | `Cmd+B` |
| Open Simulator | `Ctrl+L` | `Cmd+L` |
| Open Device Manager | `Ctrl+D` | `Cmd+D` |
| Go to Anything | `Ctrl+P` | `Cmd+P` |

---

</div>
