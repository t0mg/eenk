# Writing with eenk

Welcome to the eenk authoring guide! This document covers the eenk-specific features that extend the standard Ink language when writing stories for the Xteink X4 hardware.

## Story Header Metadata

You can define metadata for your story by adding a block comment at the very top of your main ink file using `@` tags. eenky parses these tags during compilation and bakes them into your compiled story binary so the device can display them in the library.

```ink
/*
  @title My Amazing Story
  @author John Doe
  @font my-custom-font
*/

Once upon a time...
```

- **@title**: The title of your story (maximum 63 characters).
- **@author**: The author of the story (maximum 31 characters).
- **@font**: The stem name of the font you want to use for the story (maximum 15 characters).

## Fonts in eenk

By default, eenk uses the user's preferred font setting from the device menu. However, you can force a specific font for your story by using the `@font:` metadata tag.

The runtime will attempt to resolve your requested font stem in the following order:

### 1. Built-in Fonts
The device comes with several built-in fonts that you can request directly by their token name:
- `sans` (or `sans-medium`): The default readable sans-serif font (16pt).
- `sans-small`: A smaller variant of the sans-serif font (14pt).
- `serif` (or `serif-medium`): A classic serif font (Literata, if enabled in firmware).
- `serif-large`: A larger variant of the serif font.

### 2. Custom SD Card Fonts
If the token doesn't match a built-in font, the engine will look for custom `.otf` or `.ttf` font files on your SD card. The engine searches two locations in order:
- **Story Sidecar Folder**: A folder next to your story with the same name. E.g., if your story is `/eenk/mystory.bin`, it looks in `/eenk/mystory/my-custom-font.otf`.
- **Global Fonts Folder**: A shared folder on the root of the SD card: `/fonts/my-custom-font.otf`.

### 3. Fallback
If the requested font stem cannot be found in any of the above locations, eenk will gracefully fall back to the user's device default setting.

## Building with eenky

eenky is the desktop companion application that compiles your `.ink` files into a `.bin` file optimized for the eenk hardware. It uses a customized compiler pipeline (`inklecate` -> `inkcpp_cl`).

1. Open your Ink project folder in eenky.
2. Click the **Compile** button in the toolbar.
3. eenky will automatically extract your metadata headers, compile the ink script, and generate a `.bin` file in the same directory.

## Transferring to the SD Card

To play your compiled story on the hardware device:

1. Remove the SD card from your device and plug it into your computer.
2. Ensure there is an `eenk` folder on the root of the SD card.
3. Copy the compiled `.bin` file into the `/eenk/` directory.
4. If you have custom fonts or media sidecars, place them in `/eenk/your_story_name/` (where `your_story_name` matches your `.bin` filename without the extension) or in `/fonts/`.
5. Eject the SD card, put it back in the device, and turn it on. Your story will appear in the library!
