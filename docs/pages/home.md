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

Write stories in [Ink](https://www.inklestudios.com/ink/), a markup based narrative scripting language, with the [**eenky** IDE](eenky/index.html) (or any other text editor). Use eenky to compile them for eenk, and play on the go — or on your desktop via eenky's builtin simulator.

<div class="features-grid">
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">menu_book</span>
    Ink Runtime</h3>
    <p>Powered by <strong>inkcpp</strong>, a fast C++ Ink runtime. Full Ink 1.0 support including knots, stitches, choices, variables, and functions.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">computer</span>
    eenky IDE</h3>
    <p>Author, compile, preview and transfer stories with the <strong>eenky</strong> Electron-based IDE — a fork of Inky with eenk-specific extensions.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">edit</span>
    e-Ink Display</h3>
    <p>Pixel-perfect monochrome rendering with support for dithered grayscale, custom bitmap fonts, and optimized partial refresh.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">bolt</span>
    Web Flasher</h3>
    <p>No drivers needed. Flash firmware directly from your browser using the <strong>Web Serial API</strong> — works on Chrome and Edge.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">star</span>
    Custom Fonts</h3>
    <p>Load custom fonts per-story from the SD card. Full Arabic and Thai shaping support, hyphenation, and justified text layout.</p>
  </div>
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">battery_charging_full</span>
    Battery Efficient</h3>
    <p>Deep sleep mode with latching power circuit. Hours of reading between charges on the Xteink X4's 2000 mAh battery.</p>
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
| **X4 Pro** | ESP32-S3 | SSD1677 / UC8179 | 800 × 480 | PSRAM, frontlight, A/B OTA |

## Quick Start

Ready to get started? Flash the latest firmware directly from your browser — no installation required.

<div class="hero-ctas">
  <a href="flasher/index.html" class="btn btn-primary"><span class="material-symbols-outlined">bolt</span>Flash eenk now</a>
  <a href="userguide/index.html" class="btn btn-secondary">Read user guide</a>
  <a href="eenky/index.html" class="btn btn-secondary">Write Stories</a>
</div>

</div>