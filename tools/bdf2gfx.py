#!/usr/bin/env python3
"""bdf2gfx.py — convert a BDF bitmap font to an Adafruit-GFX header in the
LemonFont.h layout: a *contiguous* glyph table over [FIRST, LAST], with empty
placeholder glyphs for codepoints the BDF doesn't define, so the renderer can
index by `cp - first`. Recreates the lost bdf2lemon.py.

Usage: bdf2gfx.py <font.bdf> <first_hex> <last_hex> <VarPrefix> > out.h
"""
import sys

def parse_bdf(path):
    glyphs = {}   # codepoint -> dict(bbx=(w,h,xo,yo), dwidth=int, bitmap=[int rows])
    asc = desc = None
    with open(path, 'r', errors='replace') as f:
        cur = None
        reading_bits = False
        for line in f:
            p = line.split()
            if not p:
                continue
            k = p[0]
            if k == 'FONT_ASCENT':  asc = int(p[1])
            elif k == 'FONT_DESCENT': desc = int(p[1])
            elif k == 'STARTCHAR':
                cur = {'cp': None, 'bbx': None, 'dwidth': None, 'bitmap': []}
            elif k == 'ENCODING':
                cur['cp'] = int(p[1])
            elif k == 'DWIDTH':
                cur['dwidth'] = int(p[1])
            elif k == 'BBX':
                cur['bbx'] = (int(p[1]), int(p[2]), int(p[3]), int(p[4]))
            elif k == 'BITMAP':
                reading_bits = True
            elif k == 'ENDCHAR':
                reading_bits = False
                if cur['cp'] is not None and cur['cp'] >= 0:
                    glyphs[cur['cp']] = cur
                cur = None
            elif reading_bits and cur is not None:
                cur['bitmap'].append(int(k, 16))
    return glyphs, asc, desc

def pack_glyph_bits(g):
    """Return a flat MSB-first, row-major bitstream of exactly w*h bits."""
    w, h, xo, yo = g['bbx']
    out = bytearray()
    acc = 0; nbits = 0
    for row in range(h):
        rowval = g['bitmap'][row] if row < len(g['bitmap']) else 0
        # BDF rows are left-justified, padded to a multiple of 8 bits.
        rowbytes = max(1, (w + 7)//8)
        for col in range(w):
            byte_i = col // 8
            bit_i = 7 - (col % 8)
            shift = (rowbytes - 1 - byte_i) * 8 + bit_i
            bit = (rowval >> shift) & 1
            acc = (acc << 1) | bit
            nbits += 1
            if nbits == 8:
                out.append(acc); acc = 0; nbits = 0
    if nbits:
        out.append(acc << (8 - nbits))
    return out

def main():
    bdf, first_s, last_s, prefix = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
    first = int(first_s, 16); last = int(last_s, 16)
    glyphs, asc, desc = parse_bdf(bdf)

    bitmaps = bytearray()
    table = []   # (bitmapOffset, w, h, xadv, xo, yo, cp, present)
    present = 0
    for cp in range(first, last + 1):
        g = glyphs.get(cp)
        if g and g['bbx'] and g['bbx'][0] > 0 and g['bbx'][1] > 0:
            w, h, xo, yo = g['bbx']
            off = len(bitmaps)
            bitmaps += pack_glyph_bits(g)
            xadv = g['dwidth'] if g['dwidth'] is not None else w
            yoff_gfx = -(yo + h)   # baseline->top, y-down (Adafruit convention)
            table.append((off, w, h, xadv, xo, yoff_gfx, cp, True))
            present += 1
        else:
            # placeholder: zero-size, advance = a space width
            table.append((len(bitmaps), 0, 0, (glyphs.get(0x20)['dwidth'] if 0x20 in glyphs else 4), 0, 0, cp, False))

    yadv = (asc or 0) + (desc or 0)
    o = []
    o.append(f"// {prefix} font, converted from {bdf.split('/')[-1]} by bdf2gfx.py")
    o.append(f"// Range: U+{first:04X}-U+{last:04X}  ({present} glyphs defined, rest placeholders)")
    o.append(f"// Metrics: ascent={asc} descent={desc}  yAdvance={yadv}")
    o.append("#pragma once")
    o.append("#include <Adafruit_GFX.h>")
    o.append("")
    o.append(f"static const uint8_t {prefix}Bitmaps[] PROGMEM = {{")
    for i in range(0, len(bitmaps), 16):
        o.append("  " + ", ".join(f"0x{b:02X}" for b in bitmaps[i:i+16]) + ",")
    o.append("};")
    o.append("")
    o.append(f"static const GFXglyph {prefix}Glyphs[] PROGMEM = {{")
    for off, w, h, xadv, xo, yo, cp, pres in table:
        tag = "" if pres else " (placeholder)"
        o.append(f"  {{ {off:5d}, {w:3d}, {h:3d}, {xadv:3d}, {xo:3d}, {yo:4d} }}, // U+{cp:04X}{tag}")
    o.append("};")
    o.append("")
    o.append(f"static const GFXfont {prefix} PROGMEM = {{")
    o.append(f"  (uint8_t*){prefix}Bitmaps,")
    o.append(f"  (GFXglyph*){prefix}Glyphs,")
    o.append(f"  0x{first:04X}, 0x{last:04X},")
    o.append(f"  {yadv}  // yAdvance")
    o.append("};")
    sys.stderr.write(f"defined={present} total={last-first+1} bitmap_bytes={len(bitmaps)} ascent={asc} descent={desc} yAdv={yadv}\n")
    print("\n".join(o))

if __name__ == '__main__':
    main()
