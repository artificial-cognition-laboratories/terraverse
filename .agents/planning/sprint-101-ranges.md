# Sprint 101 — Ranges by what built them

The first of three range sprints (102 stretched crust, 103 other worlds; glaciers
move to 104). Uses published ideas only (Davis, Suppe & Dahlen 1983 on thrust wedges).

## Built

- History: each crust cell carries the squeezing it went through, on the side
  that rode over (`over`) or was driven under (`under`); inland of a continental
  arc counts as under.
- `Tectonics`: `fold`, `plateau`, `oldness`, and `front`, cells in from the belt's
  edge. No squeeze direction is stored: a plane wave along an interpolated
  direction shears at this scale. Fold ridges are contours of `front`, so they
  follow the range.
- `World::folds`: four asymmetric ridges a cell (cells of 256 blocks or more,
  so radius about 18k up), 900 m young, 350 m old, dying out along their length.
- Plateaus: base levelled to at least 4.5 km, no further uplift, a twentieth of the erosion.
- Rivers: joins across the contours cost more in `Channels::build` (trellis
  drainage); ridges fade at the largest channels (water gaps) and are notched by streams.
- Rock: crests crop out sandstone and valleys shale, in 60 blocks of sediment.

## Not done

- Plateau interiors are level (as level as plains between cells, at about 4.35 km), but
  their broad margins are river-cut and they are small on this code (275 cells). No internal basins.
- Grid-wide erodibility by rock hardness; hardness is in the fold rock only.
- Worlds under about 18k radius have no fold ridges.
