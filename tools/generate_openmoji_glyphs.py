#!/usr/bin/env python3
"""Generate the tiny monochrome emoji table used by MiscFixedRenderer.

The input directory must contain OpenMoji's black 618x618 PNG exports.  This
script intentionally uses only Python's standard library so regenerating the
table does not add a project dependency.
"""

import argparse
import json
import struct
import zlib
from pathlib import Path


# Preserve the original trial set while expanding the collection. New entries
# are selected reproducibly from OpenMoji's metadata using the quotas below.
PINNED = {name for _, name in [
    (0x2639, "2639"), (0x270C, "270C"), (0x2728, "2728"),
    (0x2764, "2764"), (0x1F440, "1F440"), (0x1F44B, "1F44B"),
    (0x1F44C, "1F44C"), (0x1F44D, "1F44D"), (0x1F44E, "1F44E"),
    (0x1F446, "1F446"), (0x1F447, "1F447"), (0x1F448, "1F448"),
    (0x1F449, "1F449"), (0x1F44F, "1F44F"), (0x1F494, "1F494"),
    (0x1F495, "1F495"), (0x1F496, "1F496"), (0x1F499, "1F499"),
    (0x1F49A, "1F49A"), (0x1F49B, "1F49B"), (0x1F49C, "1F49C"),
    (0x1F4A9, "1F4A9"), (0x1F4AA, "1F4AA"), (0x1F4AF, "1F4AF"),
    (0x1F525, "1F525"), (0x1F5A4, "1F5A4"), (0x1F600, "1F600"),
    (0x1F602, "1F602"), (0x1F603, "1F603"), (0x1F604, "1F604"),
    (0x1F605, "1F605"), (0x1F606, "1F606"), (0x1F609, "1F609"),
    (0x1F60A, "1F60A"), (0x1F60D, "1F60D"), (0x1F60E, "1F60E"),
    (0x1F60F, "1F60F"), (0x1F610, "1F610"), (0x1F611, "1F611"),
    (0x1F612, "1F612"), (0x1F614, "1F614"), (0x1F616, "1F616"),
    (0x1F618, "1F618"), (0x1F61C, "1F61C"), (0x1F61D, "1F61D"),
    (0x1F620, "1F620"), (0x1F621, "1F621"), (0x1F622, "1F622"),
    (0x1F623, "1F623"), (0x1F624, "1F624"), (0x1F628, "1F628"),
    (0x1F629, "1F629"), (0x1F62A, "1F62A"), (0x1F62B, "1F62B"),
    (0x1F62C, "1F62C"), (0x1F62D, "1F62D"), (0x1F62E, "1F62E"),
    (0x1F630, "1F630"), (0x1F631, "1F631"), (0x1F632, "1F632"),
    (0x1F633, "1F633"), (0x1F634, "1F634"), (0x1F635, "1F635"),
    (0x1F636, "1F636"), (0x1F637, "1F637"), (0x1F641, "1F641"),
    (0x1F644, "1F644"), (0x1F648, "1F648"), (0x1F649, "1F649"),
    (0x1F64C, "1F64C"), (0x1F64F, "1F64F"), (0x1F680, "1F680"),
    (0x1F914, "1F914"), (0x1F910, "1F910"), (0x1F912, "1F912"),
    (0x1F913, "1F913"), (0x1F915, "1F915"), (0x1F917, "1F917"),
    (0x1F918, "1F918"), (0x1F922, "1F922"), (0x1F924, "1F924"),
    (0x1F927, "1F927"), (0x1F929, "1F929"), (0x1F92B, "1F92B"),
    (0x1F92C, "1F92C"), (0x1F92D, "1F92D"), (0x1F92E, "1F92E"),
    (0x1F92F, "1F92F"), (0x1F973, "1F973"), (0x1F97A, "1F97A"),
    (0x1F989, "1F989"), (0x1F995, "1F995"), (0x1F996, "1F996"),
    (0x1F9E1, "1F9E1"), (0x1F381, "1F381"), (0x1F389, "1F389"),
    (0x1F911, "1F911"), (0x1F923, "1F923"), (0x1F92A, "1F92A"),
]}

CATEGORY_QUOTAS = {
    "smileys-emotion": 100,
    "people-body": 90,
    "animals-nature": 70,
    "food-drink": 55,
    "travel-places": 50,
    "activities": 35,
    "objects": 55,
    "symbols": 40,
}

# Put practical chat and device symbols ahead of less familiar entries within
# each category. Remaining places follow Unicode/OpenMoji order.
PREFERRED = """
1F4F1 1F4BB 1F4F7 1F4FA 1F4A1 1F4D6 270F 1F512 1F513 1F511
1F528 1F527 2699 1F4B0 1F4B3 1F4E7 2709 1F4C5 1F514 1F50B 1F4FB
1F3E0 1F697 1F68C 1F682 2708 1F6B2 1F6A8 1F30D 1F30F 2600
1F319 2B50 2601 1F327 26C8 2744 1F525 1F680 26FA 1F3E5 1F3EB
26A0 2705 274C 2753 2754 2757 2755 203C 2049 2795 2796 2714
2716 27A1 2B05 2B06 2B07 25B6 23F8 23F9 23FA 1F501 1F504
26BD 1F3C0 1F3C8 1F3C9 1F3BE 1F3CF 1F3AE 1F3B2 1F3AF 1F3C6
2615 1F37A 1F37B 1F377 1F382 1F355 1F354 1F35F 1F34E 1F34C
1F353 1F349 1F36A 1F366 1F36B 1F37F 1F43E 1F436 1F431 1F428
1F998 1F42C 1F422 1F40D 1F438 1F41D 1F98B 1F339 1F33B 1F333
""".split()

# Country flags are sequences of two regional indicators. Keeping this list
# short avoids spending flash on hundreds of visually indistinguishable flags.
FLAGS = [
    ((0x1F1E6, 0x1F1FA), "1F1E6-1F1FA"),  # Australia
    ((0x1F1F3, 0x1F1FF), "1F1F3-1F1FF"),  # New Zealand
    ((0x1F1EC, 0x1F1E7), "1F1EC-1F1E7"),  # United Kingdom
    ((0x1F1FA, 0x1F1F8), "1F1FA-1F1F8"),  # United States
    ((0x1F1E8, 0x1F1E6), "1F1E8-1F1E6"),  # Canada
]

# Country artwork collapses to the same rectangular outline at 5x8. These
# hand-simplified monochrome adaptations retain a distinct visual pattern for
# each supported flag instead.
FLAG_BITMAPS = [
    [0x1C, 0x14, 0x1C, 0x01, 0x05, 0x01, 0x05, 0x00],  # AU
    [0x1C, 0x14, 0x1C, 0x00, 0x05, 0x00, 0x05, 0x00],  # NZ
    [0x11, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x11, 0x00],  # GB
    [0x1F, 0x14, 0x1F, 0x14, 0x1F, 0x01, 0x1F, 0x00],  # US
    [0x04, 0x15, 0x0E, 0x1F, 0x0E, 0x04, 0x04, 0x00],  # CA
]


def select_emoji(input_dir):
    metadata_path = input_dir.parents[1] / "data" / "openmoji.json"
    if not metadata_path.exists():
        raise FileNotFoundError(f"OpenMoji metadata is required: {metadata_path}")
    metadata = json.loads(metadata_path.read_text())
    preference = {hexcode: rank for rank, hexcode in enumerate(PREFERRED)}
    candidates = {group: [] for group in CATEGORY_QUOTAS}
    details = {}
    for entry in metadata:
        hexcode, group = entry["hexcode"], entry["group"]
        if (group not in candidates or "-" in hexcode or not entry["unicode"] or
                entry["skintone"] or not (input_dir / f"{hexcode}.png").exists()):
            continue
        details[hexcode] = entry
        candidates[group].append(hexcode)

    selected = set()
    for group, quota in CATEGORY_QUOTAS.items():
        group_pinned = [h for h in PINNED if h in details and details[h]["group"] == group]
        if len(group_pinned) > quota:
            raise ValueError(f"{group} has more pinned glyphs than its quota")
        selected.update(group_pinned)
        remaining = sorted(
            (h for h in candidates[group] if h not in selected),
            key=lambda h: (preference.get(h, len(preference)), details[h]["order"]),
        )
        selected.update(remaining[:quota - len(group_pinned)])

    missing = PINNED - selected
    if missing:
        raise ValueError(f"pinned emoji not selected: {sorted(missing)}")
    scalars = sorted((int(h, 16), h) for h in selected)
    if len(scalars) != 495:
        raise ValueError(f"expected 495 scalar emoji, found {len(scalars)}")
    return scalars


def read_indexed_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    pos, width, height, depth, colour = 8, 0, 0, 0, 0
    transparency, compressed = b"", bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, payload = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", payload[:10])
        elif kind == b"tRNS":
            transparency = payload
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    if depth != 8 or colour != 3:
        raise ValueError(f"expected an 8-bit indexed PNG: {path}")
    raw, stride, rows, prior = zlib.decompress(compressed), width, [], bytearray(width)
    offset = 0
    for _ in range(height):
        mode, scan = raw[offset], bytearray(raw[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for x in range(stride):
            left = scan[x - 1] if x else 0
            up = prior[x]
            upper_left = prior[x - 1] if x else 0
            if mode == 1:
                scan[x] = (scan[x] + left) & 0xFF
            elif mode == 2:
                scan[x] = (scan[x] + up) & 0xFF
            elif mode == 3:
                scan[x] = (scan[x] + ((left + up) >> 1)) & 0xFF
            elif mode == 4:
                p = left + up - upper_left
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upper_left)
                scan[x] = (scan[x] + (left if pa <= pb and pa <= pc else up if pb <= pc else upper_left)) & 0xFF
            elif mode != 0:
                raise ValueError(f"unsupported PNG filter {mode}: {path}")
        rows.append(scan)
        prior = scan
    return [[transparency[i] if i < len(transparency) else 255 for i in row] for row in rows]


def tiny_bitmap(alpha, target_w=5, target_h=8):
    points = [(x, y) for y, row in enumerate(alpha) for x, a in enumerate(row) if a >= 32]
    if not points:
        return [0] * target_h
    x0, x1 = min(x for x, _ in points), max(x for x, _ in points) + 1
    y0, y1 = min(y for _, y in points), max(y for _, y in points) + 1
    src_w, src_h = x1 - x0, y1 - y0
    scale = min(target_w / src_w, target_h / src_h)
    draw_w, draw_h = max(1, round(src_w * scale)), max(1, round(src_h * scale))
    left, top = (target_w - draw_w) // 2, (target_h - draw_h) // 2
    result = [0] * target_h
    for dy in range(draw_h):
        sy0 = y0 + dy * src_h // draw_h
        sy1 = y0 + (dy + 1) * src_h // draw_h
        for dx in range(draw_w):
            sx0 = x0 + dx * src_w // draw_w
            sx1 = x0 + (dx + 1) * src_w // draw_w
            total = sum(alpha[sy][sx] for sy in range(sy0, sy1) for sx in range(sx0, sx1))
            samples = max(1, (sy1 - sy0) * (sx1 - sx0))
            # OpenMoji's black set is predominantly line art. A low coverage
            # threshold preserves those strokes when an entire 618px drawing
            # is reduced to only five pixels across.
            if total >= samples * 255 * 0.15:
                result[top + dy] |= 1 << (target_w - 1 - (left + dx))
    return result


def write_header(input_dir, output):
    emoji = select_emoji(input_dir)
    entries = emoji + [(None, name) for _, name in FLAGS]
    bitmaps = []
    for _, name in emoji:
        path = input_dir / f"{name}.png"
        if not path.exists():
            raise FileNotFoundError(path)
        bitmaps.append(tiny_bitmap(read_indexed_png(path)))
    bitmaps.extend(FLAG_BITMAPS)
    lines = [
        "#pragma once", "", "#include <stdint.h>", "",
        "// Generated from OpenMoji 17.0.0 black artwork by", "// tools/generate_openmoji_glyphs.py. Each glyph is five pixels wide by", "// eight rows high and occupies the left five pixels of a 6x9 text cell.",
        f"static const uint16_t emojiGlyphCount = {len(entries)};",
        f"static const uint16_t emojiScalarGlyphCount = {len(emoji)};", "",
        "static const uint32_t emojiGlyphCodepoints[emojiScalarGlyphCount] PROGMEM = {",
    ]
    for start in range(0, len(emoji), 8):
        lines.append("  " + ", ".join(f"0x{cp:04X}" for cp, _ in emoji[start:start + 8]) + ",")
    lines += ["};", "", "static const uint8_t emojiGlyphRows[emojiGlyphCount][8] PROGMEM = {"]
    for (cp, name), rows in zip(entries, bitmaps):
        label = f"U+{cp:04X}" if cp is not None else name
        lines.append("  { " + ", ".join(f"0x{row:02X}" for row in rows) + f" }}, // {label}")
    lines += ["};", "", f"static const uint8_t emojiFlagCount = {len(FLAGS)};",
              "static const uint32_t emojiFlagFirst[emojiFlagCount] PROGMEM = {"]
    lines.append("  " + ", ".join(f"0x{seq[0]:04X}" for seq, _ in FLAGS) + ",")
    lines += ["};", "static const uint32_t emojiFlagSecond[emojiFlagCount] PROGMEM = {"]
    lines.append("  " + ", ".join(f"0x{seq[1]:04X}" for seq, _ in FLAGS) + ",")
    lines += ["};", "static const uint16_t emojiFlagGlyphIndices[emojiFlagCount] PROGMEM = {"]
    lines.append("  " + ", ".join(str(len(emoji) + i) for i in range(len(FLAGS))) + ",")
    lines += ["};", ""]
    output.write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="OpenMoji black/618x618 directory")
    parser.add_argument("output", type=Path, help="output EmojiGlyphData.h")
    args = parser.parse_args()
    write_header(args.input, args.output)


if __name__ == "__main__":
    main()
