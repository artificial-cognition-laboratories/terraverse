# Sprint 91 — Polish: plates, ice, lakes

From the planetarium after sprint 90: arcs rose as long straight bars of
land along plate edges (Earth's are island chains); plate edges ran in long
straight lines; thin stripes of one plate lay inside another; the ice cap's
edge was a circle of latitude; and on some worlds there were no lakes or
deserts at all.

## Outcome

- **Arcs are built by volcanoes.** Arc uplift is a field of cones (noise at
  frequency 36, a cell or two across) drawn at where the crust formed
  (`History::Cell::home`, carried with it), so an arc rises as a chain of
  islands and merges only where cones crowd; old arc tracks carried into
  continents are patchy, not stripes. `ARC` 0.09 at a cone's peak.
- **Plate edges wander:** the starting plates' warp is 0.30 over 5 octaves
  (was 0.20 over 3).
- **No slivers:** each step a cell with at most two neighbours of its own
  plate and two thirds of another's joins that one (nearest-cell handover
  left strips a cell or two wide). One long thin plate tongue remains on
  the default world: a plate's own shape, not a sliver.
- **Ice is the cell's own:** each cell's ice from its own cold (its band,
  its currents, its height), land needing 6 °C colder than sea (summers
  melt sheets back); ice's local chill is kept more than other albedo
  differences. **Continentality where it's cold:** half the band's frost
  again in proportion to the land round (interiors colder than their
  latitude, sea ice reaching out along cold coasts, open ocean clear).
- **Hot deserts from an 18 °C year** (Köppen's BWh; was 24 °C, which the
  energy balance's land rarely reaches): 3–8% of land on the three codes.
- **Lakes:** glacial scour (up to 24 blocks, in patches, where the yearly
  mean is below 4 °C) before erosion; small deep basins are lakes (tarns:
  6 cells, 8 blocks); lakes silt an eighth of their depth a round (a half,
  compounded, filled nearly all).
- **Placers:** heavy grains are up to 1/64 of what's worn from a range's
  core (`VEIN_CORE`, belt > 0.5; the history's belt is broad, so 0.3 took
  whole ranges), richness ×12: rich gravel on 0.6–1% of land.

TF9 still (unreleased). Golden re-pinned (336016483).

**Run:** `TestTerragen` 31/31, `TestScale`, `TestSphere`,
`TestPlanetAtlas`, `TestFar` 5/5 (the seam slit is still there; no ray
crosses it on this land), `TestFluid`, `bun check:terragen` 39/39 with the
weather's, `check:far` 100%, `check:lab`, `check:palette`.

## For the fine-grained round

Not done here, for the feature-by-feature pass: river banks and bars by bend
and rock; shores by rock (cliffs, beaches); lake edges; the far view's seam
slit; worlds with no cold land have no lakes (`TF9.YWJj`), which may be
right but deserves a look.
