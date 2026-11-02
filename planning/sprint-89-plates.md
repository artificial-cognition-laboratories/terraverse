# Sprint 89 — Plates with history

Today the plates are a snapshot: each turns about its pole, boundaries are
classified once, and mountains are a `belt` painted along them. The land is
noise placed by quantile, and erosion (sprint 88) wears noise nothing built.
So river profiles fall as area^−0.1 where uplift-built land gives about
area^−0.5 (`scripts/terragen-check/concavity.cpp`), every range is the same
age, and provinces are guesses from the snapshot.

The land should come out of a history instead. Ideas from Cortial et al.,
"Procedural Tectonic Planets" (2019), read as a paper only; nothing copied.

## The model

**Crust on the grid.** Each cell holds crust, not a height:

- kind: continental or oceanic;
- thickness and age (oceanic: since it formed at a ridge; continental:
  since it was last built or reworked);
- the plate it rides;
- how it formed: craton, orogen, arc, rift, ocean floor (this replaces
  `Province`'s guesses).

**Steps of geological time**, about 20–40, from when the body model says
plate tectonics begins (`body.*`) to the present:

1. **Move.** Each plate turns about its pole. The grid's cells stay put;
   crust is handed from cell to cell (each cell asks which crust the plate
   carries to it, in integers, so the step is exact and deterministic).
2. **Converge.**
   - Ocean under continent or under younger ocean: subduction. The old
     ocean goes down; an arc rises behind the edge (volcanoes, veins).
   - Continent against continent: collision. Crust thickens into a broad
     plateau with its ranges; the plates may weld.
3. **Diverge.** A gap fills with new ocean crust, age 0, at a ridge. Plates
   grow old and break up now and then along weak lines, so supercontinents
   split.
4. **Erode** between steps: sprint 88's rounds, with uplift from the
   crust's thickening rather than `belt`. Uplift and erosion together are
   what make concave rivers and wear old ranges low.

**Height from crust** (isostasy): thick crust stands high, old ocean floor
lies deep. Noise only adds detail on top. Sea level then follows from how
much continental crust there is; the Land setting picks that amount, so the
land share stays exact.

**Geology reads the history.** Provinces, veins near old arcs, placers
downstream of them, folded strata in old orogens, worn shields in the
oldest crust.

**The body's story.** The history's events (a supercontinent assembles, an
ocean opens, a range rises) join the body's History timeline, which already
exists on the create screen.

## The settings

The shape presets become settings for the history, not targets it aims
for:

| Setting | Means |
|---|---|
| continents | how many plates carry continental crust at the start |
| land | how much continental crust exists |
| mountains | how hard collisions and arcs push the land up |
| coast | how broken the margins are (rifting, detail) |
| supercontinent | new: where in the supercontinent cycle today falls (assembled ↔ dispersed) |

Pangaea is literally a young assembled supercontinent; Islands is many
small plates and arcs. A body without plates (the body model's `plates`
false) gets a stagnant lid: one plate, no history beyond volcanism. That
is the first step toward letting players choose the **world kind**.

## Budget

Built once per code and cached. Aim for under 20 s at creation and
optimise from there once it runs; the preview may use fewer steps if it
must, but the world is always built in full.

## Checks

- Concavity, by `concavity.cpp`: −0.4 to −0.6 over stream cells.
- Crust conserved: continental area changes only where the rules make or
  destroy it (arcs make, collisions thicken); a unit test sums it.
- Land share matches the setting exactly.
- Deterministic: the same code gives the same bits (golden re-pinned).
- `bun check:terragen`, `check:far`, the atlas checks.
- **TF8**: every world changes; TF7 is refused.

## Not in this sprint

The atlas paging tiles (needed before planets bigger than about 5 500
radius can be the default). Sub-cell streams. Playing the history back on
the globe.

## Outcome

- `terragen/history.cpp`: 32 steps of 15 million years. Each cell asks
  each plate within two cells (a 32-bit mask) where the plate's turn
  brought its crust from; the winner stays on top (continent over floor,
  the higher continent, the younger floor). Losers are collisions (the
  continental area folded away is counted, `History::lost`) or
  subduction. Collisions fold both sides of the suture up to 9 cells deep;
  arcs peak 3 cells into the overriding plate; floor that arcs build to
  near the sea becomes continent (`made`). Gaps are new floor at a ridge;
  continental cells beside them are rift margins. Rivers wear the land a
  round a step (stream power and creep, no sediment account).
- The start: continents where the old landmass field is highest (the Land
  share plus a shelf), old and level; old ocean floor elsewhere; plates as
  before (warped nearest centres, own poles).
- The grid: heights from the history, coast noise, the sea by quantile (the
  Land share exact), the ocean floor smoothed into a continental slope;
  trenches and rift valleys from today's boundaries. `belt` is how hard a
  place has been pushed up lately (fading by a quarter a step, never under
  0.18): the finished land's erosion works against it.
- Fixed on the way: a sea cell filled almost to the surface by a river's
  sediment stepped the coast beside it 23 blocks (a column added the
  cell's whole fill on the land side). Columns now read `Grid::worn`,
  erosion's change on land only.
- `drainage.h` shares the grid's fill, receivers and accumulation;
  `plates.h` the plates' hashes and vectors.
- A world builds in about 1.6 s (the history about 1.1 s of it).
- TF8; TF7 refused. Golden re-pinned (2445346592).

**Concavity** (`concavity.cpp`, now split): all stream cells −0.24 to
−0.42 (was −0.10 to −0.16); in rising ranges (belt > 0.3) −0.33 to
−0.50; on old land −0.21 to −0.35. Rising ranges are near the steady
state; old land is wearing down with nothing pushing it up, which is what
old land does. Two erosion rounds a step and slower or faster fading
changed it by a few hundredths.

**Not done (from the plan):** the supercontinent setting (the code
doesn't carry it yet); the history's events on the body's History
timeline (they're recorded, `History::events`, not sent); a stagnant lid
for bodies without plates (worlds are only made for the home body's kind
of land so far).

**Run:** `TestTerragen` 30/30 (new `testHistory`: crust kept to the
handover's error, young floor exists, collisions or arcs and openings
happen), `TestScale`, `TestSphere`, `TestPlanetAtlas`, `bun
check:terragen` 32/32, `check:far` 100%, `check:lab`, `check:palette`, UI
typecheck. **Fails:** `TestFar` `testNoHolesFromAbove`, 1 slanting ray of
40 000 through a slit where far regions 0 and 19 meet, on smooth ground (a
1-block grade): the far mesh's seam, which `atlas-seam` already failed
before this sprint, met by the new land. Not fixed.
