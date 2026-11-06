#!/usr/bin/env python3
"""Nexora asset generator: logo.bin (indexed RGBA) + font0/1/2.bin (anti-aliased glyphs).
Run:  python3 gen_assets.py logo.png     (needs: pip install pillow)"""
import sys, struct
from PIL import Image, ImageDraw, ImageFont

def make_logo(path, out="logo.bin", target_w=400):
    im = Image.open(path).convert("RGB")
    im = im.crop((90, 215, 600, 810))
    h = round(im.height * target_w / im.width)
    im = im.resize((target_w, h), Image.LANCZOS)
    px = im.load()
    rgba = Image.new("RGBA", im.size)
    rp = rgba.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b = px[x, y]
            v = max(r, g, b)
            # dark background -> transparent, bright logo -> opaque
            a = 0 if v <= 58 else 255 if v >= 105 else (v - 58) * 255 // (105 - 58)
            rp[x, y] = (r, g, b, a)
    q = rgba.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
    pal = q.getpalette(rawmode="RGBA")
    pal = (pal + [0] * 1024)[:1024]
    data = struct.pack("<HH", q.width, q.height) + bytes(pal) + q.tobytes()
    open(out, "wb").write(data)
    print(out, q.width, q.height, len(data), "bytes")

def make_font(ttf, size, chars, out):
    f = ImageFont.truetype(ttf, size)
    recs, blobs, off = [], [], 8 + 16 * len(chars)
    for ch in chars:
        l, t, r, b = f.getbbox(ch, anchor="ls")
        w, h = max(r - l, 0), max(b - t, 0)
        adv = int(round(f.getlength(ch)))
        if w and h:
            im = Image.new("L", (w, h), 0)
            ImageDraw.Draw(im).text((-l, -t), ch, font=f, fill=255, anchor="ls")
            bm = im.tobytes()
        else:
            bm = b""
        recs.append(struct.pack("<IHHhhHH", off if bm else 0, w, h, l, t, adv, 0))
        blobs.append(bm); off += len(bm)
    data = struct.pack("<II", len(chars), ord(chars[0])) + b"".join(recs) + b"".join(blobs)
    open(out, "wb").write(data)
    print(out, size, len(chars), len(data), "bytes")

if __name__ == "__main__":
    G = "/usr/share/fonts/truetype/google-fonts/"
    make_logo(sys.argv[1] if len(sys.argv) > 1 else "logo.png")
    ascii95 = "".join(chr(c) for c in range(32, 127))
    make_font(G + "Poppins-Regular.ttf", 22, ascii95, "font0.bin")
    make_font(G + "Poppins-Medium.ttf", 34, ascii95, "font1.bin")
    make_font(G + "Poppins-Light.ttf", 128, "0123456789:", "font2.bin")
