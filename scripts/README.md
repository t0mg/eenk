# eenk Scripts

This directory contains various utility scripts used for development and firmware preparation.

## Font Conversion Scripts

### `fontconvert.py`
A python script used to convert TTF/OTF font files into the `.epdfont` binary format or C++ headers used by the eenk firmware.

### `h2epdfont.py`
Converts C++ header fonts (e.g., Adafruit GFX style headers) into the `.epdfont` binary format.

### `strip_heading_font.py`
Strips unused glyphs from an `EpdFont` C++ header file, keeping only standard ASCII characters in the range `0x20-0x7E`. This is especially useful for reducing the binary footprint of all-caps display/heading fonts where you don't need international character sets or lowercase characters.

**Usage:**
```sh
python scripts/strip_heading_font.py <font_name>
```

**Example:**
```sh
python scripts/strip_heading_font.py syne_bold_10
```
*Note: The script reads the source file from `lib/EpdFont/src/builtinFonts/<font_name>.h`, and outputs the stripped version to `lib/EpdFont/src/builtinFonts/<font_name>_stripped.h`. You must manually rename the stripped file to replace the original if desired.*

## OTA Updater

### `upload_app1.py`
A small utility script to upload the OTA updater firmware to the device.
