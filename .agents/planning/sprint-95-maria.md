# Sprint 95 — Lunar volcanism and materials

The Moon's two faces: bright, saturated highlands of anorthosite, and dark
maria, basalt that welled up into the great basins long after they formed
and spread over the low ground, burying the craters under it. Later
impacts marked the maria, but far fewer. This sprint puts both on any
body whose card says so, and gives them their rocks and resources.

## Causes (never the body's name)

From the body card's history (`worlds.cpp processesOf`), on a stagnant lid:
- **flooding**, the share of the surface lava covers: from how long the
  interior stayed warm enough for volcanism (the body model's "the
  volcanoes fall silent", or the whole age where they still run):
  ¼ √(Gyr), at most ½. The Moon (silent at 0.65 Gyr) ~20%, a Mercury ~45%.
  A small body that was never volcanic (heat never reached it) floods none.
- **young**, the share of craters made after the flooding: the impact
  rate falls steeply, so the later lava keeps few: 1 / (1 + 16 t) (the
  Moon's maria about a tenth of the highlands' density).
  Never-volcanic bodies keep all their craters, all as old as the ground.

## The model

**Crust.** One side of a lid's crust is thinner (the Moon's near side): a
broad field across the body. Lava reaches the surface where the crust is
thin and a basin dug deep into it.

**Pouring.** Each basin (the stamped basin-class craters) on thin crust is
a source; the flooded share is shared among them by size and thinness.
Each pours like water rising in its basin (priority flood from the floor,
lowest edge first), level, until its share is covered; full basins spill
into the land around (the near side's great plains). Every flooded cell
stands at its pool's level.

**Burying.** Craters are old or young (before or after the flooding). The
grid stamps the old, floods, then stamps the young; a column floors the
old ones at the lava's level and cuts the young into it. Detail is
smoothed on the lava (mare plains are flat).

**Rock.** Provinces Highland and Mare. Highland: a megaregolith of
breccia over anorthosite. Mare: basalt as deep as the lava (blocks), over
breccia and anorthosite. Regolith on top of airless ground (light on
highland, dark on mare: two biomes).

**Resources.**
- Ilmenite (FeTiO₃: iron, titanium, oxygen) in mare basalt; each pool its
  own titanium (high-Ti and low-Ti maria).
- KREEP (potassium, rare earths, phosphorus; thorium) in the crust under
  and around the thin side's maria.
- Water ice in cold traps: crater floors near the poles of an airless body.

## Checks

- The Moon: maria cover about the flooded share, level (one height per
  pool), mostly on the thin side; fewer craters on them than off them.
- A never-volcanic body has none.
- Earth unchanged in its land (golden only changes for the new provinces
  if at all); TF9.

## Not this sprint

Sinuous rilles and domes (small features on the maria), wrinkle ridges,
rays: next pass. Shields and rifts on volcanic lids: later in the arc.

## Outcome

Built as planned, all from the card (`Processes::flooding`, `young`):

| body | silent at | flooded | young craters |
|---|---|---|---|
| moon | 0.65 Gyr | 19.9% (asked 20.2) | 8.8% |
| cinder | 3.15 Gyr | 45% | 2% |
| morrow | 1.65 Gyr | 32% | 4% |
| aster, vesper | never volcanic | none | all old |

(Still-volcanic lids flood half; the icy ones' "lava" is basalt for now,
until the ice-shell sprint.)

- `terragen/maria.*`: the crust's thinness (one side thinner, broad
  patches); each complex crater and basin a source by its size and the
  thinness under it (thin⁴); pours by priority flood, counting only new
  ground, so pools merge and spill. The Moon's maria sit on the thin side
  (mean thinness 0.85 against 0.51).
- Craters carry `young`; the grid stamps old, floods, stamps young; a
  column floors the old at `mare_level` and cuts the young into it
  (`World::terrain`). Detail on lava is a tenth. Earth's craters are all
  old, so its land and the golden are unchanged.
- Provinces Highland and Mare; rocks Anorthosite and Breccia (2 km of it,
  in blocks by the scale); mare basalt as deep as the lava; lava tubes in
  maria; dry caverns stay dry.
- Ores: ilmenite in mare basalt by its pool's titanium, KREEP in the
  thin side's crust, ice in polar cold traps (within 6° of a pole, crater
  floors 2 km down: a fifth of that ground). Substances `ilmenite`,
  `apatite` (Robie & Hemingway); rocks `anorthosite`, `breccia`,
  `regolith`, `mare_regolith` (origin "impact").
- Surface Regolith (airless bodies) and biomes `regolith`, `mare`, with
  their blocks (light and dark dust that holds a footprint); textures
  derived from the pack (`goodvibes.py`), palettes, `palette.json`.
- Tests: the Moon floods its share (±1%), level per pool, on the thin
  side, a quarter or less of the highlands' crater density; its rock
  columns; a never-volcanic body floods none.

Fixed after the first look in game: the maria's edges were the grid's
triangles (a column was lava wherever a flooded cell touched it, at a
level read between cells). Now a column takes its pool's level flat from
the flooded cells round it and the lava fills only the land below it, as
it stood before the flood (`World::terrain`, `Ground::mare`), so shores
follow the ground: into crater rims and up valleys. Biome and the basalt's
depth come from the same column.

Left: rilles, domes, wrinkle ridges, rays; KREEP is sparse (tune by eye
in game).
