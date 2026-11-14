#!/usr/bin/env python3
"""Nexora OS wallpaper packer.
Usage:  python3 make_walls.py img1.png img2.jpg ...   (up to 8 images, order = order in Settings)
Creates walls.bin  (appended to os.img at sector 4096 by the Makefile).
Layout (512-byte sectors, relative to walls.bin start):
   0          header: 'NXWP', count
   1 + i*64   thumbnail i   (128x72 RGB, 64 sectors)
   513 + i*2112  wallpaper i (800x450 RGB, 2112 sectors)  -> kernel scales it to the screen
"""
import sys, struct
from PIL import Image
FW, FH, TW, TH = 800, 450, 128, 72
def cover(im, w, h):
    im = im.convert('RGB'); sw, sh = im.size
    s = max(w / sw, h / sh); nw, nh = int(round(sw * s)), int(round(sh * s))
    im = im.resize((nw, nh), Image.LANCZOS)
    l, t = (nw - w) // 2, (nh - h) // 2
    return im.crop((l, t, l + w, t + h))
paths = sys.argv[1:9]
if not paths: sys.exit(__doc__)
out = bytearray(512 * (1 + 8 * 64 + 8 * 2112))
out[0:4] = b'NXWP'; out[4:8] = struct.pack('<I', len(paths))
for i, p in enumerate(paths):
    im = Image.open(p)
    th = cover(im, TW, TH).tobytes(); fu = cover(im, FW, FH).tobytes()
    o = 512 * (1 + i * 64);            out[o:o + len(th)] = th
    o = 512 * (1 + 8 * 64 + i * 2112); out[o:o + len(fu)] = fu
open('walls.bin', 'wb').write(out[:512 * (1 + 8 * 64 + (len(paths) - 1) * 2112 + 2112)] if True else out)
print('walls.bin written:', len(paths), 'wallpapers')
