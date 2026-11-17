# A crack along an atlas seam: two seam blocks' clipped edges don't meet

Split from testing-atlas-render-fails (jobs bot, 2026-11-17). `TestAtlasRender.testViewsMatchThePlanet` still
fails on one eye; the other three were the test's own ray missing two triangles' shared diagonal by float
rounding (fixed with the job's commit).

The one left: `ATLAS_EYE=58,137,22 ./.output/bin/luanti --test-module TestAtlasRender`:
`hole at el -44 az 190: drawn nothing, planet 32.7498`, planet's cell region 0 8, block (-642,-1,-1336),
seam block, sent and drawn. The nearest triangles to the ray there (eye-relative nodes):

- block (-642,-1,-1336): edge from (-23.1445, -22.72, 4.4219) to (-23.3613, -22.72, 3.4648), slanted
- block (374,-1,-1336): edge from (-23.1177, -22.72, 3.4375) to (-23.1177, -22.72, 4.4180), straight
- the ray crosses at (-23.2003, -22.75, 4.0908), between the two: a sliver about 0.1 node wide where
  neither block's top is drawn, and you see through the ground

So the two blocks either side of the seam (regions 8 and 9) are clipped to territories whose edges don't
coincide (`bendOntoAtlas` and the atlas's cut planes), leaving a gap. Players would see thin cracks of sky
or void along seams, from above.

Look at how each seam block's territory edge is placed (planet/ atlas cut planes, client
`bendOntoAtlas` in mapblock_mesh.cpp): the two sides should share one edge, or overlap by a hair.
Skipped by name in `scripts/test/skip.txt` (TestAtlasRender.testViewsMatchThePlanet) until then.

Likely shape of it (not verified): block (374,-1,-1336)'s edge lies exactly on a node boundary
(x = -23.1177 constant, unclipped), while block (-642,-1,-1336)'s is clipped to the seam plane
(`clipToTerritory`). So the strip between B's last whole node and the seam is drawn by neither: either B's
partial (cut) cell there isn't drawn, or A's territory stops short of it. Check `atlas.cut()` for the
cells either side at that point, and the test's `seam` (airlike) node, which the test's ClientWorld puts
where a block's slot is outside its territory.
