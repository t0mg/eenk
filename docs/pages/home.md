<div class="hero">
  <div class="hero-inner">
    <h1>eenk</h1>
    <p class="tagline">Interactive Fiction Firmware for Xteink e-ink devices. Run your Ink stories on beautiful monochrome hardware.</p>
    <div class="hero-ctas">
      <a href="flasher/index.html" class="btn btn-primary">⚡ Flash Your Device</a>
      <a href="installation/index.html" class="btn btn-secondary">Get Started →</a>
    </div>
  </div>
</div>

## What is eenk?

**eenk** is a custom firmware for the Xteink series of e-ink reader devices, turning them into dedicated interactive fiction players. Write stories in [Ink](https://www.inklestudios.com/ink/) using the **eenky** companion IDE, compile them to the eenk binary format, and play them on real e-ink hardware — or on your desktop via the built-in SDL simulator.

<div class="features-grid">
  <div class="feature-card">
    <span class="feature-icon">📖</span>
    <h3>Ink Runtime</h3>
    <p>Powered by <strong>inkcpp</strong>, a fast C++ Ink runtime. Full Ink 1.0 support including knots, stitches, choices, variables, and functions.</p>
  </div>
  <div class="feature-card">
    <span class="feature-icon">🖥️</span>
    <h3>eenky IDE</h3>
    <p>Author, compile, preview and transfer stories with the <strong>eenky</strong> Electron-based IDE — a fork of Inky with eenk-specific extensions.</p>
  </div>
  <div class="feature-card">
    <span class="feature-icon">🖋️</span>
    <h3>e-Ink Display</h3>
    <p>Pixel-perfect monochrome rendering with support for dithered grayscale, custom bitmap fonts, and optimized partial refresh.</p>
  </div>
  <div class="feature-card">
    <span class="feature-icon">⚡</span>
    <h3>Web Flasher</h3>
    <p>No drivers needed. Flash firmware directly from your browser using the <strong>Web Serial API</strong> — works on Chrome and Edge.</p>
  </div>
  <div class="feature-card">
    <span class="feature-icon">🔤</span>
    <h3>Custom Fonts</h3>
    <p>Load custom fonts per-story from the SD card. Full Arabic and Thai shaping support, hyphenation, and justified text layout.</p>
  </div>
  <div class="feature-card">
    <span class="feature-icon">🔋</span>
    <h3>Battery Efficient</h3>
    <p>Deep sleep mode with latching power circuit. Hours of reading between charges on the Xteink X4's 2000 mAh battery.</p>
  </div>
</div>

## Supported Hardware

<div class="device-gallery">
  <div class="device-card">
    <div class="device-img"></div>
    <div class="device-label">Xteink X3</div>
  </div>
  <div class="device-card">
    <div class="device-img"></div>
    <div class="device-label">Xteink X4</div>
  </div>
  <div class="device-card">
    <div class="device-img"></div>
    <div class="device-label">Xteink X4 Pro</div>
  </div>
</div>

| Device | MCU | Display | Resolution | Notes |
|--------|-----|---------|------------|-------|
| **X3** | ESP32-C3 | UC8253 / UC8279 | 792 × 528 | Original model |
| **X4** | ESP32-C3 | SSD1677 | 800 × 480 | Current main target |
| **X4 Pro** | ESP32-S3 | SSD1677 / UC8179 | 800 × 480 | PSRAM, frontlight, A/B OTA |

## Quick Start

<div class="info-box">
  Ready to get started? Flash the latest firmware directly from your browser — no installation required.
</div>

<div class="hero-ctas">
  <a href="flasher/index.html" class="btn btn-primary">⚡ Open Web Flasher</a>
  <a href="installation/index.html" class="btn btn-secondary">Build from Source</a>
  <a href="eenky/index.html" class="btn btn-secondary">Write Stories</a>
</div>
