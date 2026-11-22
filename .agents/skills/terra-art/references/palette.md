# The game's palette

Sampled from the game's own textures (Good Vibes, the default art): each ramp is darkest first,
ready to paste into `noise` or `grain`, or to pick palette letters from. Where a ramp has one or
two colours, that's because the game's texture is that flat: match it, don't add contrast.

What the samples say about the style:
- **Low contrast.** Rock is nearly one colour; soil and wood stay within a narrow band.
- **Warm.** Even grey stone leans yellow-green (`#9c9b85`), not blue.
- **Saturated earth.** Dirt and clay are orange-red, sand is yellow: brighter than "realistic".
- **Leaves are grey** in the texture because the game tints them by biome: for a leaf-coloured
  thing of your own, use `spruce_leaves` or `fern`.

For anything not here, render with `--with` the nearest game block and match it by eye.

## Stone & rock

| Texture | Ramp (dark → light) |
|---|---|
| stone | `"#999880", "#9c9b85"` |
| granite | `"#b67e74", "#b88379"` |
| andesite | `"#9396a8", "#979aab"` |
| basalt | `"#3e3e44", "#404046"` |
| slate | `"#525c6a", "#545e6c"` |
| limestone | `"#d2c8ab", "#d7cdaf"` |
| sandstone | `"#f3ce9a", "#f9dbb1", "#fbe8bd", "#fcf0cb"` |
| stone_bricks | `"#86876b", "#939173", "#a2a189", "#b1af98"` |
| obsidian | `"#321939", "#381d3f", "#3e2146", "#46244e"` |
| tuff | `"#252525", "#2f2f2f", "#343434"` |

## Soil & loose

| Texture | Ramp (dark → light) |
|---|---|
| dirt | `"#7d361f", "#994a2d", "#ab5d39", "#a8653b"` |
| grass_block_top | `"#9da435", "#a2a83a", "#b0b641"` |
| sand | `"#fdce56", "#fdd355", "#ffde55", "#ffe26b"` |
| red_sand | `"#fe9968", "#fe9e67", "#ffa768", "#ffb27b"` |
| gravel | `"#422817", "#472d1c", "#513320", "#754b2f"` |
| clay | `"#bf6334", "#d27036", "#d88238", "#df9541"` |
| mud | `"#4e2b1d", "#593321", "#5e3d25", "#65472c"` |
| snow | `"#c3dae2", "#cbdee5", "#d4e3e8", "#d9e8ed"` |
| terracotta | `"#af8847", "#b18a4b"` |

## Wood

| Texture | Ramp (dark → light) |
|---|---|
| oak_log | `"#532a1c", "#572d20", "#6b3a22", "#6a3e23"` |
| oak_log_top | `"#623321", "#c0a15b", "#d8ba74", "#e1c37d"` |
| birch_log | `"#4e453a", "#929e75", "#aab09a", "#bec9a3"` |
| spruce_log | `"#48201b", "#4d211b", "#56271c", "#59291c"` |
| jungle_log | `"#72471f", "#785524", "#7b5b24", "#826d24"` |
| acacia_log | `"#443829", "#4c402f", "#544735", "#5f553e"` |

## Leaves & plants

| Texture | Ramp (dark → light) |
|---|---|
| oak_leaves | `"#7d7d7d", "#919191", "#a7a7a7", "#b0b0b0"` |
| spruce_leaves | `"#546645", "#566a4b", "#718261"` |
| birch_leaves | `"#e54035", "#f84931", "#fb553f", "#fc7161"` |
| fern | `"#818b6f", "#8a9577", "#9daa88", "#b1c099"` |
| reed | `"#63361d", "#71442b", "#a5784e", "#af8258"` |

## Ore (the mineral's own fleck colour shows in the light steps)

| Texture | Ramp (dark → light) |
|---|---|
| coal_ore | `"#393822", "#999880", "#9c9b85", "#a3a28b"` |
| iron_ore | `"#908f76", "#9c9b85", "#e1cda9"` |
| copper_ore | `"#c06d3e", "#999880", "#9c9b85"` |
| tin_ore | `"#999880", "#9c9b85", "#d2d2c8"` |
| gold_ore | `"#908f76", "#9c9b85", "#eac44c"` |
