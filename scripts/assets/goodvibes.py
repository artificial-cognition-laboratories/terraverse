#!/usr/bin/env python3
"""
Good Vibes textures for Terra Firma's blocks, from the pinned revision of
the pack (CC BY 4.0, by Acaitart; see assets/CREDITS.md).

Copies are the pack's PNGs unchanged. Derived textures (rocks and ores the
pack doesn't have) are made from the pack's own art, as listed in DERIVED:
ore specks recoloured onto its stone, rocks retinted from its granite,
andesite and stone. Run it again to regenerate; it caches downloads under
.output/assets-cache.

    python3 scripts/assets/goodvibes.py
"""
import pathlib
import urllib.request

from PIL import Image

REV = "f1bb6fb20e36a47b8dacbdf23c4a10b994b6f224"
BASE = f"https://raw.githubusercontent.com/Phyronnaz/VoxelAssets/{REV}/GoodVibes/"
ROOT = pathlib.Path(__file__).resolve().parents[2]
CACHE = ROOT / ".output" / "assets-cache"
OUT = ROOT / "apps" / "game" / "game" / "mods" / "tf_world" / "textures"
BLOCK = "minecraft/textures/block/"
BOP = "biomesoplenty/blocks/"

# target name (tf_goodvibes_<name>.png) → pack path, copied unchanged
COPIES = {
    "granite": BLOCK + "granite.png",
    "andesite": BLOCK + "andesite.png",
    "obsidian": BLOCK + "obsidian.png",
    "iron_ore": BLOCK + "iron_ore.png",
    "gold_ore": BLOCK + "gold_ore.png",
    "coal_ore": BLOCK + "coal_ore.png",
    "lava_still": BLOCK + "lava_still.png",
    "lava_flow": BLOCK + "lava_flow.png",
    "clay": BLOCK + "clay.png",
    "red_sand": BLOCK + "red_sand.png",
    "terracotta": BLOCK + "terracotta.png",
    "tuff": BOP + "ash_block.png",
    "mud": BOP + "mud.png",
    # trees
    "birch_log": BLOCK + "birch_log.png", "birch_log_top": BLOCK + "birch_log_top.png", "birch_leaves": BLOCK + "birch_leaves.png",
    "spruce_log": BLOCK + "spruce_log.png", "spruce_log_top": BLOCK + "spruce_log_top.png",
    "jungle_log": BLOCK + "jungle_log.png", "jungle_log_top": BLOCK + "jungle_log_top.png", "jungle_leaves": BLOCK + "jungle_leaves.png",
    "acacia_log": BLOCK + "acacia_log.png", "acacia_log_top": BLOCK + "acacia_log_top.png", "acacia_leaves": BLOCK + "acacia_leaves.png",
    "palm_log": BOP + "palm_log.png", "palm_log_top": BOP + "palm_log_top.png", "palm_leaves": BOP + "leaves_palm.png",
    "willow_log": BOP + "willow_log.png", "willow_log_top": BOP + "willow_log_top.png", "willow_leaves": BOP + "leaves_willow.png",
    "cactus_side": BLOCK + "cactus_side.png", "cactus_top": BLOCK + "cactus_top.png",
    # small plants
    "grass": BLOCK + "grass.png", "fern": BLOCK + "fern.png", "dead_bush": BLOCK + "dead_bush.png",
    "dandelion": BLOCK + "dandelion.png", "poppy": BLOCK + "poppy.png", "orange_tulip": BLOCK + "orange_tulip.png",
    "white_tulip": BLOCK + "white_tulip.png", "lavender": BOP + "lavender.png",
    "brown_mushroom": BLOCK + "brown_mushroom.png", "red_mushroom": BLOCK + "red_mushroom.png",
    "reed": BOP + "reed.png", "cattail": BOP + "cattail.png", "desert_grass": BOP + "desertgrass.png",
    "dune_grass": BOP + "dunegrass.png", "berry_bush": BOP + "berrybush.png",
}


def fetch(path):
    local = CACHE / path
    if not local.exists():
        local.parent.mkdir(parents=True, exist_ok=True)
        with urllib.request.urlopen(BASE + path) as r:
            local.write_bytes(r.read())
    return Image.open(local).convert("RGBA")


def lum(px):
    return 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2]


def tint(src, colour, gain=1.0):
    """The source's light and shade, in a new colour."""
    img = src.copy()
    p = img.load()
    mean = sum(lum(p[x, y]) for x in range(img.width) for y in range(img.height)) / (img.width * img.height)
    for y in range(img.height):
        for x in range(img.width):
            k = lum(p[x, y]) / mean * gain
            p[x, y] = tuple(max(0, min(255, int(c * k))) for c in colour) + (p[x, y][3],)
    return img


def specks(ore, stone):
    """Which pixels of an ore texture are ore (they differ from its stone)."""
    a, b = ore.load(), stone.load()
    return [[abs(lum(a[x, y]) - lum(b[x, y])) > 18 or max(abs(a[x, y][c] - b[x, y][c]) for c in range(3)) > 30
             for y in range(ore.height)] for x in range(ore.width)]


def recolour_ore(ore, stone, host, colour):
    """The ore's specks, in a new colour, set into a host texture."""
    mask = specks(ore, stone)
    img = host.copy()
    p, o = img.load(), ore.load()
    shade = [lum(o[x, y]) for x in range(ore.width) for y in range(ore.height) if mask[x][y]]
    mean = sum(shade) / max(1, len(shade))
    for y in range(ore.height):
        for x in range(ore.width):
            if mask[x][y]:
                k = lum(o[x, y]) / mean
                p[x, y] = tuple(max(0, min(255, int(c * k))) for c in colour) + (255,)
    return img


def derived():
    stone, granite, andesite = fetch(BLOCK + "stone.png"), fetch(BLOCK + "granite.png"), fetch(BLOCK + "andesite.png")
    diorite, iron, gold, gravel = fetch(BLOCK + "diorite.png"), fetch(BLOCK + "iron_ore.png"), fetch(BLOCK + "gold_ore.png"), fetch(BLOCK + "gravel.png")
    return {
        # rocks the pack doesn't draw, retinted from ones it does
        "basalt": tint(andesite, (64, 64, 70)),
        "gneiss": tint(granite, (150, 140, 132)),
        "slate": tint(stone, (84, 94, 108)),
        "shale": tint(stone, (118, 104, 94)),
        "limestone": tint(stone, (214, 204, 174)),
        "salt": tint(diorite, (238, 228, 224)),
        # a cratered moon's: the highlands' pale crust and its shattered top,
        # and the dust over both (light) and over the maria (dark)
        "anorthosite": tint(diorite, (222, 222, 218)),
        "breccia": tint(gravel, (150, 148, 144)),
        "regolith": tint(stone, (172, 170, 166)),
        "mare_regolith": tint(stone, (92, 92, 96)),
        # ores: the pack's iron ore specks, recoloured, in its stone
        "copper_ore": recolour_ore(iron, stone, stone, (196, 112, 64)),
        "tin_ore": recolour_ore(iron, stone, stone, (214, 214, 204)),
        "lead_ore": recolour_ore(iron, stone, stone, (92, 98, 124)),
        "magnetite_ore": recolour_ore(iron, stone, tint(andesite, (64, 64, 70)), (34, 34, 40)),
        "sulfur_ore": recolour_ore(gold, stone, stone, (238, 220, 48)),
        "ilmenite_ore": recolour_ore(iron, stone, tint(andesite, (64, 64, 70)), (24, 22, 30)),
        "kreep_ore": recolour_ore(gold, stone, tint(diorite, (222, 222, 218)), (190, 140, 96)),
        # river gravel with the gold ore's flecks in it
        "placer_gravel": recolour_ore(gold, stone, gravel, (232, 196, 60)),
    }


DERIVED = {
    "basalt": "andesite, retinted dark grey",
    "gneiss": "granite, retinted grey-brown",
    "slate": "stone, retinted blue-grey",
    "shale": "stone, retinted grey-brown",
    "limestone": "stone, retinted cream",
    "salt": "diorite, retinted off-white",
    "anorthosite": "diorite, retinted pale grey",
    "breccia": "gravel, retinted grey",
    "regolith": "stone, retinted light grey",
    "mare_regolith": "stone, retinted dark grey",
    "copper_ore": "iron ore's specks recoloured copper, on stone",
    "tin_ore": "iron ore's specks recoloured pale grey, on stone",
    "lead_ore": "iron ore's specks recoloured blue-grey, on stone",
    "magnetite_ore": "iron ore's specks recoloured black, on retinted andesite",
    "sulfur_ore": "gold ore's specks recoloured yellow, on stone",
    "ilmenite_ore": "iron ore's specks recoloured black, on retinted andesite",
    "kreep_ore": "gold ore's specks recoloured tan, on retinted diorite",
    "placer_gravel": "gold ore's specks set into gravel",
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, path in COPIES.items():
        img = fetch(path)
        if img.width > 128:  # the pack's animated lava is 512 px a frame: kept to 64 (flowing: 128)
            w = 128 if name == "lava_flow" else 64
            img = img.resize((w, img.height * w // img.width), Image.NEAREST)
        img.save(OUT / f"tf_goodvibes_{name}.png")
    for name, img in derived().items():
        img.save(OUT / f"tf_goodvibes_{name}.png")
    print(f"{len(COPIES)} copied, {len(DERIVED)} derived → {OUT}")


if __name__ == "__main__":
    main()
