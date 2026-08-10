<div class="hero">
  <div class="hero-inner">
    <h1>eenk</h1>
    <p class="tagline">Interactive Fiction Firmware for Xteink e-ink devices.<br>
    Run stories written in <a href="https://www.inklestudios.com/ink/">inkle's Ink</a> in the palm of your hand.</p>
    <div class="hero-ctas">
      <a href="flasher/index.html" class="btn btn-primary"><span class="material-symbols-outlined">bolt</span> Flash Your Device</a>
      <a href="userguide/index.html" class="btn btn-secondary">Get Started →</a>
    </div>
  </div>
</div>

<div class="page-content">

## What is eenk?

**eenk** is a custom firmware for the Xteink series of ESP32 powered e-ink reader devices, turning them into dedicated interactive fiction players.

Write stories in [Ink](https://www.inklestudios.com/ink/), a markup based narrative scripting language, with the [**eenky** IDE](eenky/index.html) (or any other text editor). Use eenky to compile them for eenk, and play on the go or on your desktop via eenky's builtin simulator.

<div class="features-grid">
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">favorite</span>
    Pocketable Adventures</h3>
    <p>Bring Interactive Fiction to your pocket and enjoy lofi, distraction-free e-paper adventures on a device you already love.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">menu_book</span>
    Ink Runtime</h3>
    <p>Stories are written in <a href="https://www.inklestudios.com/ink/">Ink</a>, a markup based narrative scripting language, and played thanks to <a href="https://github.com/JBenda/inkcpp">inkcpp</a>, a fast C++ compiler and runtime.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">computer</span>
    eenky IDE</h3>
    <p>Create stories with the <a href="eenky/">eenky IDE</a>, a modernized fork of <a href="https://github.com/inkle/inky">Inky</a> made specifically for eenk, with a few extra tricks up its sleeve.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">brand_family</span>
    Custom Fonts</h3>
    <p>Custom font can be applied per story or globally. Uses the text layout and rendering pipeline of <a href="https://github.com/bigbag/papyrix-reader">Papyrix firmware</a>.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">wall_art</span>
    Image support</h3>
    <p>Cover art, thumbnails and inlined images can be bundled with your story. Images are handled using the same pattern as Inky's web export.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">bolt</span>
    Web Flasher</h3>
    <p>No drivers, no Wifi, no taking out the SD card. Flash the firmware and manage your story library from your browser over USB.</p>
  </div>
</div>

## Supported Hardware

<div class="device-gallery">
  <div class="device-card">
    <div class="device-label">Xteink X3 (untested!)</div>
    <div class="device-img">
      <model-viewer src="https://overseas-static-file.xteink.cc/public/web/models/X3-En-Black.glb?v=20260422-4" alt="Xteink X3 3D Model" interaction-prompt="none" auto-rotate camera-controls shadow-intensity="1" touch-action="none"></model-viewer>
    </div>
  </div>
  <div class="device-card">
    <div class="device-label">Xteink X4</div>
    <div class="device-img">
      <model-viewer src="https://overseas-static-file.xteink.cc/public/web/models/X4-En-Black.glb?v=20260422-4" alt="Xteink X4 3D Model" interaction-prompt="none" auto-rotate camera-controls shadow-intensity="1" touch-action="none"></model-viewer>
    </div>
  </div>
  <div class="device-card">
    <div class="device-label">Xteink X4 Pro</div>
    <div class="device-img">
      <img src="assets/images/x4pro.png">
    </div>
  </div>
</div>

| Device | MCU | Display | Resolution | Notes |
|--------|-----|---------|------------|-------|
| **X3** | ESP32-C3 | UC8253 / UC8279 | 792 × 528 | Original model |
| **X4** | ESP32-C3 | SSD1677 | 800 × 480 | Current main target |
| **X4 Pro** | ESP32-S3 | SSD1677 / UC8179 | 800 × 480 | Brings a lot more RAM, backlight, and touch |

## Quick Start

Ready to get started? Flash the latest firmware directly from your browser, no installation required.

<div class="hero-ctas">
  <a href="flasher/index.html" class="btn btn-primary"><span class="material-symbols-outlined">bolt</span>Flash eenk now</a>
  <a href="userguide/index.html" class="btn btn-secondary">Read user guide</a>
  <a href="eenky/index.html" class="btn btn-secondary">Write Stories</a>
</div>

</div>