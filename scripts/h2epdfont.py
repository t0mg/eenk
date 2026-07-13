#!/usr/bin/env python3
"""
h2epdfont.py — Convert an EENK builtin .h font header to a binary .epdfont file.

Usage:
    python scripts/h2epdfont.py <header.h> [<output.epdfont>]

If no output path is given, the .epdfont is placed alongside the .h file.

Binary .epdfont format (matches EpdFontLoader):
  FileHeader (16 bytes):
    magic       uint32  0x46445045  ("EPDF" LE)
    version     uint16  1
    flags       uint16  bit0 = is2Bit
    reserved    uint8[8]

  FileMetrics (14 bytes):
    advanceY    uint8
    padding     uint8
    ascender    int16
    descender   int16
    intervalCount uint32
    glyphCount  uint32
    bitmapSize  uint32

  Intervals: intervalCount × (first uint32, last uint32, offset uint32)
  Glyphs:    glyphCount    × (width uint8, height uint8, advanceX uint8,
                               padding uint8, left int16, top int16,
                               dataLength uint16, dataOffset uint32)
             total = 14 bytes per glyph
  Bitmap:    bitmapSize bytes
"""

import sys
import re
import struct
import os

# ─── Header parser ────────────────────────────────────────────────────────────

def parse_u8_array(src: str, name: str) -> bytes:
    """Extract a C uint8_t[] array by name and return its bytes."""
    # Match:  const uint8_t ... <name>Bitmaps[...] = { ... };
    # We search for the array open and then collect all hex/decimal values.
    pat = re.compile(
        r'(?:PROGMEM\s+)?' + re.escape(name) + r'Bitmaps\s*\[\d+\]\s*=\s*\{(.*?)\}',
        re.DOTALL
    )
    m = pat.search(src)
    if not m:
        raise ValueError(f"Could not find bitmap array '{name}Bitmaps'")
    raw = m.group(1)
    values = [int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+|\d+', raw)]
    return bytes(values)


def parse_glyph_array(src: str, name: str):
    """
    Parse EpdGlyph array.
    Each entry is an initialiser list: {width, height, advanceX, left, top, dataLength, dataOffset}
    Returns list of 7-tuples.
    """
    pat = re.compile(
        r'(?:PROGMEM\s+)?' + re.escape(name) + r'Glyphs\s*\[\]\s*=\s*\{(.*?)\};',
        re.DOTALL
    )
    m = pat.search(src)
    if not m:
        raise ValueError(f"Could not find glyph array '{name}Glyphs'")
    body = m.group(1)

    # Match each glyph: {w, h, advX, left, top, dataLen, dataOff}
    glyph_pat = re.compile(
        r'\{\s*'
        r'(-?[0-9]+)\s*,\s*'   # width
        r'(-?[0-9]+)\s*,\s*'   # height
        r'(-?[0-9]+)\s*,\s*'   # advanceX
        r'(-?[0-9]+)\s*,\s*'   # left
        r'(-?[0-9]+)\s*,\s*'   # top
        r'([0-9]+)\s*,\s*'     # dataLength
        r'([0-9]+)\s*'         # dataOffset
        r'\}'
    )
    glyphs = []
    for gm in glyph_pat.finditer(body):
        w, h, ax, left, top, dlen, doff = [int(x) for x in gm.groups()]
        glyphs.append((w, h, ax, left, top, dlen, doff))
    if not glyphs:
        raise ValueError(f"Could not parse any glyphs in '{name}Glyphs'")
    return glyphs


def parse_interval_array(src: str, name: str):
    """
    Parse EpdUnicodeInterval array.
    Each entry: {first, last, offset}
    Returns list of 3-tuples.
    """
    pat = re.compile(
        r'(?:PROGMEM\s+)?' + re.escape(name) + r'Intervals\s*\[\]\s*=\s*\{(.*?)\};',
        re.DOTALL
    )
    m = pat.search(src)
    if not m:
        raise ValueError(f"Could not find intervals array '{name}Intervals'")
    body = m.group(1)

    iv_pat = re.compile(
        r'\{\s*'
        r'(0x[0-9A-Fa-f]+|\d+)\s*,\s*'  # first
        r'(0x[0-9A-Fa-f]+|\d+)\s*,\s*'  # last
        r'(0x[0-9A-Fa-f]+|\d+)\s*'       # offset (may be hex!)
        r'\}'
    )
    intervals = []
    for im in iv_pat.finditer(body):
        first  = int(im.group(1), 0)
        last   = int(im.group(2), 0)
        offset = int(im.group(3), 0)
        intervals.append((first, last, offset))
    return intervals


def parse_font_struct(src: str, name: str):
    """
    Parse the top-level EpdFontData struct: advanceY, ascender, descender, is2Bit.
    """
    # Match the struct initialiser for the font
    # e.g.  const EpdFontData reader_medium_2b = { ..., advanceY, ascender, descender, is2Bit };
    # The struct fields: bitmap, glyph, intervals, intervalCount, advanceY, ascender, descender, is2Bit
    pat = re.compile(
        r'const\s+EpdFontData\s+' + re.escape(name) + r'\s*=\s*\{(.*?)\};',
        re.DOTALL
    )
    m = pat.search(src)
    if not m:
        raise ValueError(f"Could not find EpdFontData struct for '{name}'")
    body = m.group(1)

    # Extract numeric tokens (skip array/struct refs which aren't numbers)
    nums = re.findall(r'-?\d+', body)
    if len(nums) < 4:
        raise ValueError(f"Not enough numeric fields in EpdFontData for '{name}'")
    # Fields order (from EpdFontData): bitmap* (skip), glyph* (skip), intervals* (skip),
    #   intervalCount, advanceY, ascender, descender, is2Bit
    # After removing the pointer fields (they start with &), we want:
    # intervalCount, advanceY, ascender, descender, is2Bit
    # But the regex will pick up any embedded numbers in symbol names - safer to look for
    # the last fields which are always plain integers.
    # Strategy: find "intervalCount" by scanning comma-separated segments
    tokens = [t.strip() for t in body.split(',')]
    # Drop leading pointer refs (&foo)
    numeric_tokens = []
    for t in tokens:
        t_clean = t.strip().lstrip('0').strip()
        # Keep only tokens that are purely numeric (possibly negative)
        if re.match(r'^-?\d+$', t.strip()):
            numeric_tokens.append(int(t.strip()))

    if len(numeric_tokens) < 4:
        raise ValueError(f"Cannot extract advanceY/ascender/descender/is2Bit from struct '{name}'")

    # Last 4 numeric values are: advanceY, ascender, descender, is2Bit
    intervalCount = numeric_tokens[0]
    advanceY      = numeric_tokens[1]
    ascender      = numeric_tokens[2]
    descender     = numeric_tokens[3]
    is2Bit        = bool(numeric_tokens[4]) if len(numeric_tokens) > 4 else ('2b' in name)

    return advanceY, ascender, descender, is2Bit, intervalCount


# ─── Writer ───────────────────────────────────────────────────────────────────

MAGIC   = 0x46445045  # "EPDF" LE
VERSION = 1
GLYPH_SIZE = 14       # matches EpdFontLoader binary glyph record


def write_epdfont(out_path: str, bitmap: bytes, glyphs: list, intervals: list,
                  advanceY: int, ascender: int, descender: int, is2Bit: bool):
    flags = 0x0001 if is2Bit else 0x0000
    bitmap_size = len(bitmap)
    glyph_count = len(glyphs)
    interval_count = len(intervals)

    with open(out_path, 'wb') as f:
        # FileHeader
        f.write(struct.pack('<IHH8s', MAGIC, VERSION, flags, b'\x00' * 8))
        # FileMetrics
        f.write(struct.pack('<BBhh III',
                            advanceY & 0xFF, 0,        # advanceY, padding
                            ascender, descender,        # int16 each
                            interval_count, glyph_count, bitmap_size))
        # Intervals
        for first, last, offset in intervals:
            f.write(struct.pack('<III', first, last, offset))
        # Glyphs  (width u8, height u8, advanceX u8, padding u8, left i16, top i16, dataLen u16, dataOff u32)
        for w, h, ax, left, top, dlen, doff in glyphs:
            f.write(struct.pack('<BBBBhhHI', w, h, ax, 0, left, top, dlen, doff))
        # Bitmap
        f.write(bitmap)

    total = os.path.getsize(out_path)
    print(f"  Wrote {out_path}  ({total:,} bytes)")
    print(f"    advanceY={advanceY}  ascender={ascender}  descender={descender}  2bit={is2Bit}")
    print(f"    intervals={interval_count}  glyphs={glyph_count}  bitmap={bitmap_size:,}")


# ─── Main ─────────────────────────────────────────────────────────────────────

def convert(h_path: str, out_path=None):
    print(f"Converting: {h_path}")
    with open(h_path, 'r', encoding='utf-8', errors='replace') as f:
        src = f.read()

    # Derive the font name from the header comment or filename
    # "generated by fontconvert.py\n * name: reader_medium_2b"
    m = re.search(r'\*\s*name:\s*(\S+)', src)
    if m:
        font_name = m.group(1)
    else:
        font_name = os.path.splitext(os.path.basename(h_path))[0]

    print(f"  Font name: {font_name}")

    bitmap    = parse_u8_array(src, font_name)
    glyphs    = parse_glyph_array(src, font_name)
    intervals = parse_interval_array(src, font_name)
    advanceY, ascender, descender, is2Bit, _ = parse_font_struct(src, font_name)

    if out_path is None:
        out_path = os.path.splitext(h_path)[0] + '.epdfont'

    write_epdfont(out_path, bitmap, glyphs, intervals, advanceY, ascender, descender, is2Bit)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    h_file = sys.argv[1]
    out_file = sys.argv[2] if len(sys.argv) > 2 else None
    try:
        convert(h_file, out_file)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
