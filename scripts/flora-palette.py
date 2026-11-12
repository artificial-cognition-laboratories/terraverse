#!/usr/bin/env python3
# The plant blocks' palette (tf_world's tf_flora_palette.png): 16 hues × 16
# shades, the same table as apps/game/src/mapgen/terra_flora.cpp's palette(),
# which picks a lineage's colours from it. And the plant blocks' greyed
# textures, made from the Good Vibes pack's, for the palette to colour.
import colorsys, sys
from PIL import Image

tex = sys.argv[1] if len(sys.argv) > 1 else "apps/game/game/mods/tf_world/textures"
S = [0.12, 0.35, 0.6, 0.85]
V = [0.28, 0.48, 0.7, 0.92]
img = Image.new("RGB", (16, 16))
for i in range(256):
    h, s, v = (i % 16) / 16, S[(i // 16) % 4], V[i // 64]
    sector = int(h * 6) % 6
    f = h * 6 - int(h * 6)
    p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    rgb = [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)][sector]
    img.putpixel((i % 16, i // 16), tuple(round(c * 255) for c in rgb))
img.save(f"{tex}/tf_flora_palette.png")

def grey(name, out, lift, alpha_under=110):
    im = Image.open(f"{tex}/tf_goodvibes_{name}.png").convert("RGBA")
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            l = min(255, int((0.3 * r + 0.59 * g + 0.11 * b) * lift))
            if a < 128:  # (opaque: a cut shape is the leaf's outline)
                l, a = alpha_under, 255
            px[x, y] = (l, l, l, a)
    im.save(f"{tex}/{out}.png")

grey("oak_log", "tf_flora_bark", 2.6)
grey("oak_log_top", "tf_flora_wood", 1.5)
grey("oak_leaves", "tf_flora_leaf", 1.45, 150)
grey("cactus_side", "tf_flora_flesh", 2.0)
