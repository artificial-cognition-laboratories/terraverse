# Water doesn't carry across an atlas seam: the probe finds air past it

Found 2026-11-21 while fixing the player's invisible wall at seams. `bash apps/game/tests/checks/atlas-seam.sh`
fails 6 of 517 on master as committed (`acb8b622b`), before that change and after it alike (it runs on the
server; the change was the client body's):

```
ATLAS miss a segment's seam at longitude 0, east at the equator: past the seam is air, not water
ATLAS miss a segment's seam, east at the equator: past the seam is air, not water
ATLAS miss a segment's seam, east near 45°: past the seam is air, not water
ATLAS miss a segment's seam, west near 30° south: past the seam is air, not water
ATLAS miss into the cap at 0°: past the seam is air, not water
ATLAS miss into the cap at 45°: past the seam is air, not water
```

Every walker, sand and other water case passes: only water read across a seam, at a segment's seam and
into the cap, comes out air. So the check is red on master.

Start at the probe (`apps/game/src/probe/`, `--probe atlas-seams`): what "past the seam" it reads (the node
across by `Map::see`, or a fill), then whether the water's there in the map (the generator's sea at the
cells either side) and the probe's read is wrong, or the water's missing (the matter's fills not following
the seam, or the generator's water level differing across regions).

## Agent notes (2026-11-24, jobs bot)

Half of it was the probe (fixed, committed "atlas-seam probe: pour water as the matter now has it"):
it set one `core:water_source` without waking the matter, and read node names, but water is now a
fill in an air node. The check is 514/3 now. The 3 left are real:

    a segment's seam at longitude 0, east at the equator / into the cap at 0° / into the cap at 45°

At each, the map's storage jumps at the seam (e.g. node x −10011 → −18935), and the matter's flow
(`library/matter/sandbox`: flow.cpp, pouring.cpp, world.cpp step to `pos ± 1`) never looks across;
water reaches the last node before the seam and stops. Where storage carries on (segments 1, 5)
it crosses. Fixing it is a feature, not a night's fix: `WorldMatter::Ground` needs a neighbour
hook (`next(Pos, dir)` → the node(s) across, `Map::see`/`seeAll`, with the grid's turn and the
non-aligned overlap of cells), every neighbour step in the sandbox routed through it, and mass
conserved where one cell meets two. Heat and gases share the same stepping, so they don't cross
either. Needs the developer's go-ahead as a piece of work.
