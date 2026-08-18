#!/usr/bin/env python3
"""
epdfont2h.py — Convert a binary .epdfont file into a C++ EpdFontData header (.h).

Usage:
    python scripts/epdfont2h.py <font.epdfont> <output_header.h> <font_symbol_name>
"""

import sys
import struct
import os

MAGIC = 0x46445045  # "EPDF" LE

def epdfont_to_header(epd_path: str, h_path: str, font_name: str):
    with open(epd_path, 'rb') as f:
        data = f.read()

    if len(data) < 30:
        raise ValueError("File too small to be a valid .epdfont")

    magic, version, flags = struct.unpack_from('<IHH', data, 0)
    if magic != MAGIC:
        raise ValueError(f"Invalid magic: {hex(magic)} (expected {hex(MAGIC)})")

    is_2bit = (flags & 0x0001) != 0

    advanceY, padding, ascender, descender, interval_count, glyph_count, bitmap_size = struct.unpack_from(
        '<BBhhIII', data, 16
    )

    offset = 16 + struct.calcsize('<BBhhIII')  # 34 bytes

    intervals = []
    for _ in range(interval_count):
        first, last, iv_offset = struct.unpack_from('<III', data, offset)
        intervals.append((first, last, iv_offset))
        offset += 12

    glyphs = []
    for _ in range(glyph_count):
        w, h, ax, _, left, top, dlen, doff = struct.unpack_from('<BBBBhhHI', data, offset)
        glyphs.append((w, h, ax, left, top, dlen, doff))
        offset += 14

    bitmap = data[offset:offset + bitmap_size]
    if len(bitmap) != bitmap_size:
        raise ValueError(f"Bitmap size mismatch: read {len(bitmap)}, expected {bitmap_size}")

    print(f"Read {epd_path}:")
    print(f"  font_name: {font_name}")
    print(f"  advanceY={advanceY}, ascender={ascender}, descender={descender}, 2bit={is_2bit}")
    print(f"  intervals={interval_count}, glyphs={glyph_count}, bitmap={bitmap_size} bytes")

    # Generate C header
    lines = []
    lines.append("/**")
    lines.append(" * generated from .epdfont via epdfont2h.py")
    lines.append(f" * name: {font_name}")
    lines.append(f" * mode: {'2-bit' if is_2bit else '1-bit'}")
    lines.append(" */")
    lines.append("#pragma once")
    lines.append('#include "EpdFontData.h"')
    lines.append("")
    lines.append(f"static const uint8_t PROGMEM {font_name}Bitmaps[{bitmap_size}] = {{")

    # Format bitmap bytes in rows of 16
    chunk_size = 16
    for i in range(0, len(bitmap), chunk_size):
        chunk = bitmap[i:i + chunk_size]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        if i + chunk_size < len(bitmap):
            lines.append(f"    {hex_vals},")
        else:
            lines.append(f"    {hex_vals}")
    lines.append("};")
    lines.append("")

    lines.append(f"static const EpdGlyph PROGMEM {font_name}Glyphs[] = {{")
    for w, h, ax, left, top, dlen, doff in glyphs:
        lines.append(f"    {{{w}, {h}, {ax}, {left}, {top}, {dlen}, {doff}}},")
    lines.append("};")
    lines.append("")

    lines.append(f"static const EpdUnicodeInterval PROGMEM {font_name}Intervals[] = {{")
    for first, last, iv_offset in intervals:
        lines.append(f"    {{0x{first:X}, 0x{last:X}, {iv_offset}}},")
    lines.append("};")
    lines.append("")

    lines.append(f"const EpdFontData {font_name} = {{")
    lines.append(f"    {font_name}Bitmaps,")
    lines.append(f"    {font_name}Glyphs,")
    lines.append(f"    {font_name}Intervals,")
    lines.append(f"    {interval_count},")
    lines.append(f"    {advanceY},")
    lines.append(f"    {ascender},")
    lines.append(f"    {descender},")
    lines.append(f"    {'true' if is_2bit else 'false'}")
    lines.append("};")
    lines.append("")

    os.makedirs(os.path.dirname(os.path.abspath(h_path)), exist_ok=True)
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))

    print(f"Wrote {h_path} ({os.path.getsize(h_path):,} bytes)")

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: python scripts/epdfont2h.py <font.epdfont> <output_header.h> <font_symbol_name>")
        sys.exit(1)
    epdfont_to_header(sys.argv[1], sys.argv[2], sys.argv[3])
