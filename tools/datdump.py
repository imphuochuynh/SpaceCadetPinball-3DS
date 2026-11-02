#!/usr/bin/env python3
"""Inspect a Space Cadet PINBALL.DAT / CADET.DAT file (PARTOUT 4.0 format).

Dev tool for layout work: dumps the background bitmap at a given resolution
as PNG and prints the score-box / text-box rectangles the game uses.
Usage: datdump.py CADET.DAT out_dir [resolution]
"""
import os
import struct
import sys

from PIL import Image

FIELD_SIZE = [2, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0]
SHORT, BMP8, GROUPNAME, PALETTE, SHORTARRAY, BMP16 = 0, 1, 3, 5, 10, 12


def load(path):
    data = open(path, "rb").read()
    sig = data[:21].split(b"\0")[0]
    assert sig == b"PARTOUT(4.0)RESOURCE", sig
    full_tilt = os.path.basename(path).upper().startswith("CADET")
    num_groups, _body, unknown = struct.unpack_from("<HiH", data, 21 + 50 + 100 + 4)
    pos = 183 + unknown
    groups = []
    for _ in range(num_groups):
        count = data[pos]; pos += 1
        entries = []
        for _ in range(count):
            etype = data[pos]; pos += 1
            fixed = FIELD_SIZE[etype]
            if fixed >= 0:
                size = fixed
            else:
                size = struct.unpack_from("<I", data, pos)[0]; pos += 4
            entries.append((etype, data[pos:pos + size]))
            pos += size
        groups.append(entries)
    return groups, full_tilt


def name_of(group):
    for etype, buf in group:
        if etype == GROUPNAME:
            return buf.split(b"\0")[0].decode("latin1")
    return None


def bitmaps(group):
    out = []
    for etype, buf in group:
        if etype == BMP8:
            res, w, h, x, y, size, flags = struct.unpack_from("<BhhhhiB", buf, 0)
            out.append(dict(res=res, w=w, h=h, x=x, y=y, flags=flags, pixels=buf[14:14 + size]))
    return out


def find(groups, name):
    for i, g in enumerate(groups):
        if name_of(g) == name:
            return i
    return -1


def short_arrays(group):
    return [struct.unpack("<%dh" % (len(b) // 2), b) for t, b in group if t == SHORTARRAY]


def main():
    groups, full_tilt = load(sys.argv[1])
    out_dir = sys.argv[2]
    res = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    os.makedirs(out_dir, exist_ok=True)

    bg = groups[find(groups, "background")]
    pal = next(b for t, b in bg if t == PALETTE)
    # Stored as ARGB-ish 4-byte entries; the game swaps to RGBA. Byte order in file: B,G,R,A (Windows RGBQUAD)
    palette = []
    for i in range(256):
        b, g, r, _a = pal[i * 4:i * 4 + 4]
        palette += [r, g, b]

    for bmp in bitmaps(bg):
        if bmp["res"] != res:
            continue
        w, h = bmp["w"], bmp["h"]
        stride = w if w % 4 == 0 else w - w % 4 + 4
        img = Image.new("P", (w, h))
        img.putpalette(palette)
        rows = [bmp["pixels"][r * stride:r * stride + w] for r in range(h)]
        img.putdata(b"".join(reversed(rows)))  # DIB rows are bottom-up
        img.convert("RGB").save(os.path.join(out_dir, "background_res%d.png" % res))
        print("background res%d: %dx%d at (%d,%d) flags=%d" % (res, w, h, bmp["x"], bmp["y"], bmp["flags"]))

    for label in ("score1", "ballcount1", "player_number1"):
        gi = find(groups, label)
        arrays = short_arrays(groups[gi + res]) if gi >= 0 else []
        print(label, gi, arrays[:2])

    for label in ("info_text_box", "mission_text_box"):
        gi = find(groups, label)
        if gi < 0:
            print(label, "missing"); continue
        for arr in short_arrays(groups[gi + res]):
            if arr and arr[0] == 1500:
                print(label, arr)


if __name__ == "__main__":
    main()
