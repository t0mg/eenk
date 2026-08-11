<div class="hero">
  <div class="hero-inner">
    <h1>eenk</h1>
    <p class="tagline">Interactive Fiction Firmware for Xteink e-ink devices.<br>
    Run stories written in <a href="https://www.inklestudios.com/ink/">inkle's Ink</a> in the palm of your hand.</p>
    <div class="hero-ctas">
      <a href="flasher/" class="btn btn-primary"><span class="material-symbols-outlined">bolt</span> Flash Your Device</a>
      <a href="userguide/" class="btn btn-secondary">Get Started →</a>
    </div>
  </div>
</div>

<div class="page-content">

<div class="critical-warning" role="alert">
  <div class="warning-title">
    <span class="material-symbols-outlined warning-icon">warning</span>
    WAIT!
  </div>
  <p>This website, the eenk firmware and the eenky IDE are currently in active development. You can browse these pages but the firmware binaries and installer files are not available yet. Please stay tuned!</p>
</div>

## What is eenk?

**eenk** is a custom firmware for the Xteink series of ESP32 powered e-ink reader devices, turning them into dedicated interactive fiction players.

Write interactive stories with the [**eenky** IDE](eenky/) (or any other text editor), compile them for eenk using eenky's build pipeline, and play on the go or on your desktop on the built-in simulator.

<div class="features-grid">
  <div class="feature-card">
    <h3><span class="feature-icon material-symbols-outlined">favorite</span>
    Pocketable Adventures</h3>
    <p>Bring Interactive Fiction to your pocket and enjoy a new kind of mobile gaming: lofi, distraction-free e-paper adventures!</p>
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
      <model-viewer src="assets/glb/X3-En-Black.glb" alt="Xteink X3 3D Model" interaction-prompt="none" auto-rotate camera-controls shadow-intensity="1" touch-action="pan-y"></model-viewer>
    </div>
  </div>
  <div class="device-card">
    <div class="device-label">Xteink X4</div>
    <div class="device-img">
      <model-viewer src="assets/glb/X4-En-White.glb" alt="Xteink X4 3D Model" interaction-prompt="none" auto-rotate camera-controls shadow-intensity="1" touch-action="pan-y"></model-viewer>
    </div>
  </div>
  <div class="device-card">
    <div class="device-label">Xteink X4 Pro</div>
    <div class="device-img">
      <img src="assets/images/x4pro.png">
    </div>
  </div>
</div>

| Device | MCU | Resolution | Notes |
|--------|-----|------------|-------|
| **X3** | ESP32-C3 | 792 × 528 | Original model |
| **X4** | ESP32-C3 | 800 × 480 | Current main target |
| **X4 Pro** | ESP32-S3 | 800 × 480 | Extra RAM and backlight |

## Quick Start

Ready to get started? Flash the latest firmware directly from your browser, no installation required.

<p class="hero-ctas">
  <a href="flasher/" class="btn btn-primary"><span class="material-symbols-outlined">bolt</span>Flash eenk now</a>
  <a href="userguide/" class="btn btn-secondary">Read user guide</a>
  <a href="eenky/" class="btn btn-secondary">Write Stories</a>
</p>

</div>