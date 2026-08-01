# Writing with eenk

Welcome to the eenk authoring guide! This document covers the eenk-specific features that extend the standard Ink language when writing stories for the Xteink X4 hardware.

## Story Header Metadata

You can define metadata for your story by adding a block comment at the very top of your main ink file using `@` tags. eenky parses these tags during compilation and bakes them into your compiled story binary (`.bin` file) so the device can display them in the library.

```ink
/*
  @title My Amazing Story
  @author John Doe
  @font my-custom-font
  @cover cover.png
  @thumbnail thumb.png
*/

Once upon a time...
```

- **@title**: The title of your story (maximum 63 characters).
- **@author**: The author of the story (maximum 31 characters).
- **@font**: The stem name of the font you want to use for the story (maximum 15 characters). See [Fonts in eenk](#fonts-in-eenk) below.
- **@cover**: The cover image will automatically be resized to fit the device screen (480x800) for the sleep screen. Using this tag will create a `.media` file alongside your compiled story, see [Images in eenk](#images-in-eenk) below.
- **@thumbnail**: A separate image path to be used as a 156x156 thumbnail in the library list. If not provided, the cover image will be resized and used instead. Using this tag will also create the `.media` file.

> Note: Metadata tags are all optional. The whole metadata header is, too: you don't have to provide it to compile an ink story for eenk.

## Fonts in eenk

By default, eenk uses the user's preferred font setting from the device menu. However, you can force a specific font for your story by using the `@font` metadata tag.

The runtime will attempt to resolve your requested font stem in the following order:

### 1. Built-in Fonts
The device comes with several built-in fonts that you can request directly by their token name:
- `sans` (or `sans-medium`): The default readable sans-serif font (16pt).
- `sans-small`: A smaller variant of the sans-serif font (14pt).
- `serif` (or `serif-medium`): A classic serif font (Literata, if enabled in firmware).
- `serif-large`: A larger variant of the serif font.

### 2. Custom SD Card Fonts
If the token doesn't match a built-in font, the engine will look for custom `.epdfont` font files on your SD card. The engine searches two locations in order:
- **Story Folder**: Next to your story file. E.g., if your story is `/eenk/mystory/mystory.bin`, it looks in `/eenk/mystory/my-custom-font.epdfont`.
- **Global Font Folder**: A shared folder on the root of the SD card: `/fonts/my-custom-font.epdfont`.

### 3. Variants for Custom Fonts
Font variants may also be provided by adding a suffix to the font stem. If the story contains bold text, the engine will look for `my-custom-font-bold.epdfont`. Supported suffixes are `-bold`, `-italic`, `-bolditalic`.

If the engine cannot find those variants, synthetic variants are generated from the base font as a fallback (but these are lower quality).

### 4. Fallback
If the requested font stem cannot be found in any of the above locations, eenk will gracefully fall back to the user's device default setting, or the builtin font.

## Images in eenk

You can embed images in your story using the standard Ink `# IMAGE:` tag. eenky will automatically process these images during compilation and bundle them into an optimized `.media` sidecar file.

```ink
# IMAGE: my-image.png
# IMAGE: https://example.com/online-image.jpg
```

- **Local Images:** You can reference local images by placing them in the same folder as your `.ink` file (or a subfolder relative to it).
- **Online Images:** You can also provide a direct URL to an online image. eenky will download it automatically during compilation.

### The Media Sidecar

During the compilation process, eenky collects all the `# IMAGE:` tags it finds in your story, processes the images (scaling, dithering, and converting to 1-bit format for the e-ink display), and packs them into a single binary file named after your story with the `.media` extension (e.g., `main.media`).

When transferring your story to the device using the Device Manager in eenky, this `.media` file is automatically transferred alongside your main `.bin` story file and any custom `.epdfont` files.

> Note: eenky inherits the classic Web export mode from Inky, which also supports the very same `# IMAGE:` tag format for rendering images in the browser! What are the odds!?

## Building with eenky

eenky is the desktop companion application that compiles your `.ink` files into a `.bin` file optimized for the eenk hardware. It uses a customized compiler pipeline (`inklecate` -> `inkcpp_cl`).

1. Open your Ink project folder in eenky.
2. Click the **Compile** button in the toolbar (also available from the File menu or with Ctrl+B or Cmd+B keyboard shortcut).

eenky will automatically extract your metadata headers, compile the ink script, and generate a `.bin` file in the same directory. It will also convert the source `.ttf` fonts to `.epdfont` files and generate a `.media` file if there are any `# IMAGE:` tags in your story.

You can play it in eenky's simulator by clicking the device button in the toolbar (also available from the Device menu or with Ctrl+L or Cmd+L keyboard shortcut).

## Transferring to the eenk device

To play your compiled story on the hardware device you can either take the SD card out and put it in your computer, or transfer directly from eenky to the device if it has USB Serial capability.

### Using the Device Manager in eenky

1. Wake up your eenk device and put it into the menu screen.
2. Connect it to your computer via USB.
3. Open eenky's **Device Manager** from the home screen, Device menu or keyboard shortcut (Ctrl+D or Cmd+D). 
4. Click **Connect** and select the correct COM port (e.g. `USB jtag/serial debug unit`).
5. Click **Upload Story** and select your story `.bin` file. Associated files like fonts and `.media` will be added automatically.
6. Click **Disconnect** and the device will reboot to reveal your new story in the library!

### Manual write to the SD card

1. Remove the SD card from your device and plug it into your computer.
2. Ensure there is an `eenk` folder on the root of the SD card.
3. Copy the compiled `.bin` file into the `/eenk/` directory, or in a subfolder in this directory (useful if there are additional files such as fonts).
4. If you have custom fonts or a `.media` file, place them next to our story file (fonts can also go in the `/fonts/` folder).
5. Eject the SD card, put it back in the device, and turn it on. Your story will appear in the library!
