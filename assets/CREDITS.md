# Asset credits

## Music

The local development menu plays **“If Gaia Was One Of Us Part Four” by Denovaire**. The source is [Denovaire on Uppbeat](https://uppbeat.io/contributors/denovaire). The other music files currently in `assets/music/` are **“The Farwatch Plains” by Ian Aisling** ([artist](https://uppbeat.io/contributors/ian-aisling)) and **“Perspectives” by Kevin MacLeod** ([track](https://uppbeat.io/music/tracks/kevin-macleod/perspectives)).

These files do not have a redistributable licence documented in this repository. They are excluded from production UI builds until permission to include the audio in open-source game downloads is recorded here. Attribution alone does not grant that permission.

## Interface fonts

**Silkscreen** by Jason Kottke and **JetBrains Mono** by JetBrains are bundled through `@fontsource`, both under the SIL Open Font License. They are used for display lettering and interface text, respectively.

## Terra Firma default block textures

Good Vibes by Acaitart, licensed under [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).

Source: [Phyronnaz/VoxelAssets, GoodVibes](https://github.com/Phyronnaz/VoxelAssets/tree/f1bb6fb20e36a47b8dacbdf23c4a10b994b6f224/GoodVibes), revision `f1bb6fb20e36a47b8dacbdf23c4a10b994b6f224`.

Terra Firma copies selected original PNG files with `tf_goodvibes_` prefixes and uses them for its block faces. The PNG artwork is unchanged. Dry grass and tundra apply colour tints at runtime. Animated water uses the source pack's water texture. A copy of the source licence is in `games/terrafirma/mods/tf_world/GOODVIBES-LICENSE.txt`.

Since sprint 9 the textures are fetched by `scripts/assets/goodvibes.py` from the same pinned revision. Most are copied unchanged (rocks, ores, lava, clay, trees, leaves, plants, from the pack's `minecraft` and `biomesoplenty` folders); the animated lava is scaled down. Some are **derived** from the pack's own art (changes made, as CC BY 4.0 requires noting):

- basalt, gneiss, slate, shale, limestone, salt: the pack's andesite, granite, stone and diorite, retinted;
- copper, tin, lead, magnetite and sulfur ores: the pack's iron or gold ore specks recoloured, on its stone (magnetite on retinted andesite);
- gold-bearing gravel: the gold ore's specks set into the pack's gravel.
- anorthosite, breccia, regolith and mare regolith (sprint 95): the pack's diorite, gravel and stone, retinted;
- ilmenite and KREEP ores: the iron or gold ore's specks recoloured, on retinted andesite or diorite.

Several leaf and plant textures are greyscale in the pack (meant to be tinted per biome); the game multiplies them by a green at runtime.


## Weather particles

`tf_weather_rain.png`, `tf_weather_snow.png` and `tf_weather_ash.png` (in `games/terrafirma/mods/tf_weather/textures`) are original to Terra Firma: a few pixels each, drawn for this game in sprint 12.

## Fire tools

`tf_matter_fire_drill.png`, `tf_matter_bellows_side.png` and `tf_matter_bellows_front.png` (in `games/terrafirma/mods/tf_matter/textures`) are original to Terra Firma, drawn for this game in sprint 17. Charcoal and fired clay retint the Good Vibes gravel and clay (derived, as above).

## Substances and lab tools

`tf_matter_substance.png`, `tf_matter_liquid.png`, `tf_matter_heat_gun.png` and `tf_matter_probe.png` (in `games/terrafirma/mods/tf_matter/textures`) are original to Terra Firma, drawn for this game in sprint 18; substance blocks tint the first two per substance.

`tf_flight_pack.png` (in `games/terrafirma/mods/tf_flight/textures`) is original to Terra Firma, drawn for this game in sprint 20.

## tf_tools item icons

`apps/game/game/mods/tf_tools/textures/`: drawn for Terra Firma, part of the game.
