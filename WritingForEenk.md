# Writing for eenk

Welcome to the eenk authoring guide! This chapter covers the eenk-specific features that extend the standard Ink language when writing stories for the eenk firmware.

## Story Header Metadata

You can define metadata for your story by adding a block comment at the very top of your main ink file using `@` tags. eenky parses these tags during compilation and bakes them into your compiled story package (`.eenk` file) so the device can display them in the library.

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
- **@cover**: The cover image will automatically be resized to fit the device screen (480x800) for the sleep screen. Using this tag will pack the image into the `.media` sidecar inside your `.eenk` package, see [Images in eenk](#images-in-eenk) below.
- **@thumbnail**: A separate image path to be used as a 156x156 thumbnail in the library list. If not provided, the cover image will be resized and used instead. Using this tag will also pack the image into the `.media` sidecar.

> Note: Metadata tags are all optional. The whole metadata header is, too: you don't have to provide it to compile an ink story for eenk.

## Fonts in eenk

By default, eenk uses the user's preferred font setting from the device menu. However, you can set a specific font for your story by using the `@font` metadata tag.

The runtime will attempt to resolve the font defined by the metadata tag in the following order: built-in, custom bundled, custom global (see below). Note that users can also choose to override the `@font` tag via firmware settings.

### Built-in Fonts
The device comes with built-in fonts that you can request directly by their token name:
- `sans` (or `sans-medium`): The default sans-serif font, Reader, from the Papyrix firmware.
- `sans-small`: A smaller variant of the sans-serif font.
- `serif` (or `serif-medium`): The classic serif font (Literata).
- `serif-small`: A smaller variant of the serif font.

### Custom Fonts
If the token doesn't match a built-in font, the engine will look for custom `.epdfont` font files, first in the story bundle, then in the global `fonts/` directory on the SD card.

### Variants for Custom Fonts
Font variants may also be provided by adding a suffix to the font stem. If the story contains bold text, the engine will look for `my-custom-font-bold.epdfont`. Supported suffixes are `-bold`, `-italic`, `-bolditalic`.

If the engine cannot find those variants, synthetic variants are generated from the base font as a fallback (but these are lower quality).

### Fallback
If the requested font stem cannot be found in any of the above locations, eenk will gracefully fall back to the user's device default setting, or the builtin font.

## Text Formatting

eenk supports inline text styling in both narrative text and interactive choice options. You can use standard Markdown syntax or HTML tags.

> Note: If you use formatting with a custom font, consider providing the necessary font variants (e.g., `my-custom-font-bold.epdfont`) in your story bundle. While eenk will automatically generate synthetic fallbacks (algorithmic emboldening and obliquing), having dedicated files ensures the best quality. See [Variants for Custom Fonts](#variants-for-custom-fonts).

### Markdown Formatting

| Style | Syntax | Example |
|-------|--------|---------|
| **Italic** | `*text*` or `_text_` | `*whispers quietly*` |
| **Bold** | `**text**` or `__text__` | `**DANGER AHEAD**` |
| **Bold & Italic** | `***text***` or `**_text_**` | `***CRITICAL ALERT***` |

### HTML Tags (Backwards Compatibility)

For backwards compatibility with classic Ink stories and Inky's HTML export, standard HTML formatting tags are also supported:

- `<i>...</i>` or `<em>...</em>` for *italic* text
- `<b>...</b>` or `<strong>...</strong>` for **bold** text
- Nested tags such as `<b><i>...</i></b>` for ***bold italic*** text
- `<br>`, `<br/>`, or `<br />` to insert explicit line breaks within paragraphs

```ink
The guard stepped forward. <b>"Halt!"</b> he yelled.<br><i>He did not look pleased.</i>

+ [**Fight** the guard] -> fight
+ [<i>Reason</i> with him] -> reason
```

## Images in eenk

You can embed images in your story using the standard Ink `# IMAGE:` tag. eenky will automatically process these images during compilation and bundle them into an optimized `.media` sidecar file inside your `.eenk` story package.

```ink
# IMAGE: my-image.png
# IMAGE: https://example.com/online-image.jpg
```

- **Local Images:** You can reference local images by placing them in the same folder as your `.ink` file (or a subfolder relative to it).
- **Online Images:** You can also provide a direct URL to an online image. eenky will download it automatically during compilation.

> Note: eenky inherits the classic Web export mode from Inky, which also supports the very same `# IMAGE:` tag format for rendering images in the browser! What are the odds!?

## Checkpoints and Chapters

eenk supports special tags to let players save snapshots of their progress and rewind during gameplay via the on-device **Story Menu** (accessed by pressing `BACK` or `Escape` in the simulator):

### Checkpoints (`# CHECKPOINT`)

Use `# CHECKPOINT` without a title when you want to offer an ephemeral quick-save or safe point where players can **rewind once** (for instance, right before a deadly combat choice, tricky riddle, or branching decision).

```ink
=== before_the_trap ===
# CHECKPOINT
You stand before three levers. A skull is etched above the center one.
+ [Pull the left lever] -> left_lever
+ [Pull the center lever] -> center_lever
+ [Pull the right lever] -> right_lever
```

- Each occurrence of `# CHECKPOINT` updates the player's quick-save to that exact position. 
- There is no visual clue that a checkpoint has been reached, it is left to you whether or not to reveal it (for example with an explicit `[checkpoint]`, or a subtler in-game message).
- In the Story Menu, this is presented as **`Rewind to last checkpoint`**.

### Chapters (`# CHECKPOINT: <Title>`)

Use `# CHECKPOINT: <Title>` when you want players to be able to **choose how far back to rewind** across the story (e.g., acts, chapters, or major branching milestones).

```ink
=== act_one ===
# CHECKPOINT: Act I - The Heist
The neon signs of Sector 4 flickered against the perpetual rain...

...

=== act_two ===
# CHECKPOINT: Act II - The Escape
Sirens echoed in the distance as the hovercraft engine roared to life...
```

- Each named checkpoint is added chronologically to the Story Menu, under **`Rewind to...`**.
- Rewinding to a chapter will remove access to subsequent ones: it's a linear chain and you snip it when you rewind.
- There is no visual clue that a chapter has been reached, it is left to you whether or not to reveal it (for example with an explicit `Act I`, or a subtler in-game message).

## Building with eenky

eenky compiles your `.ink` files into a `.eenk` package file optimized for the eenk hardware. It uses a customized compiler pipeline (`inklecate` -> `inkcpp_cl` -> `eenkPackage`).

1. Open your Ink project folder in eenky.
2. Click the **Build** button in the toolbar (also available from the File menu or with Ctrl+B or Cmd+B keyboard shortcut).

eenky will automatically extract your metadata headers, compile the ink script, convert source `.ttf` fonts to `.epdfont` files, dither and pack images into `.media`, and bundle everything into a single `.eenk` story package.

You can play it immediately in eenky's simulator by clicking the device button in the toolbar (also available from the Device menu or with Ctrl+L or Cmd+L keyboard shortcut). This creates a temporary package in the simulator but does not save it.

## Transferring to the eenk device

To play your compiled story on the hardware device, use the USB Device Manager (recommended) or copy files manually via SD card.

### Using the Device Manager

1. Wake up your eenk device and remain on the library screen.
2. Connect it to your computer via USB-C.
3. Open the **Device Manager** in eenky (Ctrl+D / Cmd+D) or use the [Web Device Manager](https://t0mg.github.io/eenk/device-manager/).
4. Click **Connect** and select the serial port.
5. Drag and drop your `.eenk` package file into the upload zone (or click **Upload Story** and select the `.eenk` file). The Device Manager will unpack, verify, display the cover preview, and stream all story assets into `/stories/<story_name>/` automatically.
6. Click **Disconnect** and the device will refresh to reveal your new story in the library!

### Manual Transfer to SD Card

Direct SD card copying is not recommended because the device firmware requires unpacked story files in subfolders rather than the single `.eenk` archive:

1. Rename `mystory.eenk` to `mystory.zip` and extract it on your computer.
2. Remove the MicroSD card from your device and connect it to your computer.
3. Create a folder under `/stories/` on the SD card (e.g. `/stories/mystory/`).
4. Copy `story.bin` and any companion files (`.media` for image data, `.epdfont` for custom fonts) into `/stories/mystory/`.
5. Eject the SD card, re-insert it into your device, and power on.

## Sharing Your Stories

You can, of course, share the `.eenk` file for others to upload by dragging it into the Device Manager.

There are also a couple of alternatives for an even smoother installation experience.

### One-Click Install Deeplinks

You can also skip the manual download phase by providing a one-click installation link formatted as such:
```
https://t0mg.github.io/eenk/device-manager?url=https://example.com/mystory.eenk
```

Where `https://example.com/mystory.eenk` is the location of your compiled story file.

#### Limitations

For this to work, you need a host that allows any origin in their [CORS headers](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Access-Control-Allow-Origin).

If you don't have a personal website, or don't know what CORS headers are, a good place to host your story for free is [GitHub](https://github.com/). Just provide the link to the file on GitHub in the `url` query parameter, and the Device Manager will handle the rest (via [jsdelivr](https://www.jsdelivr.com/) in this case).

Typically, if you prefer to host your story on a place like [itch.io](https://itch.io/), the download will be blocked. In that case, submitting the Story to the showcase page (see below) is a viable alternative.

### Stories Showcase

The [online Stories Showcase](https://t0mg.github.io/eenk/stories/) also hosts free stories with a 1-click install button.

If you would like to add your story there, just [open an issue on GitHub](https://github.com/t0mg/eenk/issues/new).