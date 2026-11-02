#!/usr/bin/env python3
"""Render top-screen previews from corners.cpp output.
Usage: render.py table.png corners.txt out_dir"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw

def coeffs(src, dst):
    # PIL wants the map from output pixel -> input pixel
    A, b = [], []
    for (x, y), (u, v) in zip(dst, src):
        A.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); b.append(u)
        A.append([0, 0, 0, x, y, 1, -v * x, -v * y]); b.append(v)
    return np.linalg.solve(np.array(A, float), np.array(b, float)).tolist()

table = Image.open(sys.argv[1]).convert("RGBA")
tw, th = table.size
src = [(0, 0), (tw, 0), (tw, th), (0, th)]  # TL TR BR BL
os.makedirs(sys.argv[3], exist_ok=True)
for line in open(sys.argv[2]):
    parts = line.split()
    name, nums = parts[0], list(map(float, parts[1:]))
    dst = [(nums[i], nums[i + 1]) for i in range(0, 8, 2)]
    img = Image.new("RGBA", (400, 240), (0, 0, 0, 255))
    warped = table.transform((400, 240), Image.PERSPECTIVE, coeffs(src, dst), Image.BILINEAR)
    img.alpha_composite(warped)
    img.convert("RGB").save(os.path.join(sys.argv[3], name + ".png"))
