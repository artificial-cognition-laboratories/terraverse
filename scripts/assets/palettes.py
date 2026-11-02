#!/usr/bin/env python3
"""The colour palettes for blocks whose param2 is a composition.

- Soil (tf_world_soil_palette.png): entry i is soil point i's colour (the
  points come from terragen, printed by soil_points.cpp): sand pale, silt
  grey-brown, clay reddish, organic matter darkening it toward peat. The
  soil texture is a neutral grey version of the pack's dirt, so the palette
  sets its colour.
- Rocks (tf_world_<rock>_palette.png): entry k blends the rock's look toward
  its partner's (granite toward gneiss…), k = 0 unchanged to 15 most
  blended; a multiply can only darken, so each is scaled to its brightest.

Run from the repo root after the textures are fetched (goodvibes.py)."""
import subprocess, sys, tempfile, os
from PIL import Image

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
tex = os.path.join(root, "apps/game/game/mods/tf_world/textures")

def average(path):
    im = Image.open(path).convert("RGB")
    px = [im.getpixel((x, y)) for y in range(im.size[1]) for x in range(im.size[0])]
    return tuple(sum(p[i] for p in px) / len(px) for i in range(3))

def palette(colours, name):
    im = Image.new("RGB", (16, 16), (255, 255, 255))
    for i, c in enumerate(colours):
        im.putpixel((i % 16, i // 16), tuple(max(0, min(255, round(v))) for v in c))
    im.save(os.path.join(tex, name))

# --- soil: a neutral texture, and a colour per point
dirt = Image.open(os.path.join(tex, "tf_goodvibes_dirt.png")).convert("RGB")
grey = Image.new("RGB", dirt.size)
for y in range(dirt.size[1]):
    for x in range(dirt.size[0]):
        r, g, b = dirt.getpixel((x, y))
        v = min(255, int((r + g + b) / 3 * 1.6))
        grey.putpixel((x, y), (v, v, v))
grey.save(os.path.join(tex, "tf_world_soil.png"))
base = average(os.path.join(tex, "tf_world_soil.png"))

with tempfile.TemporaryDirectory() as work:
    exe = os.path.join(work, "soil_points")
    subprocess.run(["g++", "-std=c++17", "-O1", "-I" + os.path.join(root, "apps/game/src")] +
                   [os.path.join(root, "apps/game/src/terragen", f) for f in os.listdir(os.path.join(root, "apps/game/src/terragen")) if f.endswith(".cpp")] +
                   [os.path.join(root, "scripts/assets/soil_points.cpp"), "-o", exe], check=True)
    points = [list(map(float, line.split()[1:])) for line in subprocess.run([exe], capture_output=True, text=True, check=True).stdout.splitlines()]

SAND, SILT, CLAY, PEAT = (222, 200, 150), (150, 132, 108), (168, 108, 82), (52, 40, 30)
soil = []
for sand, silt, clay, organic in points:
    mineral = [SAND[i] * sand + SILT[i] * silt + CLAY[i] * clay for i in range(3)]
    dark = min(1.0, organic * 1.8)
    target = [mineral[i] * (1 - dark) + PEAT[i] * dark for i in range(3)]
    soil.append([255 * target[i] / base[i] for i in range(3)])
palette(soil, "tf_world_soil_palette.png")

# --- rocks: each toward its partner (geological neighbours)
PARTNERS = {"granite": "gneiss", "gneiss": "slate", "slate": "shale", "basalt": "andesite", "andesite": "tuff",
            "tuff": "andesite", "obsidian": "basalt", "shale": "limestone", "limestone": "sandstone",
            "anorthosite": "breccia", "breccia": "anorthosite"}
for rock, partner in PARTNERS.items():
    own, other = average(os.path.join(tex, f"tf_goodvibes_{rock}.png")), average(os.path.join(tex, f"tf_goodvibes_{partner}.png"))
    ratio = [other[i] / own[i] for i in range(3)]
    top = max(ratio + [1.0])
    ratio = [r / top for r in ratio]
    colours = []
    for k in range(16):
        t = k / 15
        colours.append([255 * (1 - t + t * ratio[i]) for i in range(3)])
    palette(colours, f"tf_world_{rock}_palette.png")
print("palettes: soil (%d points), %d rocks" % (len(points), len(PARTNERS)))
