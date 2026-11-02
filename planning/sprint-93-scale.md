# Sprint 93 — Scale

Relief is about 70 blocks on every planet (`LAND_SCALE` 160 blocks per
unit of elevation, `SEA_SCALE` 64), and the fine detail's wavelength is the
planet's size / 64. So a big planet is today's small one magnified: an
Earth-sized world has hills hundreds of km wide and 30 m tall, and the
create screen says "bigger worlds are flatter".

## The model

**Heights in metres.** A unit of the grid's elevation is 5 km (what the
lapse rate already says: 0.9 of heat is ~33 °C, Earth's 6.5 °C/km over 5 km).
A body has its **blocks per metre**, `s = √(R / R⊕)` with R in blocks and
R⊕ = 6 371 000 (an Earth-sized world is a block a metre), at most 1:

| radius | s | 1 unit (5 km) | was |
|---:|---:|---:|---:|
| 2.6k | 0.020 | 101 blocks | 160 |
| 10k | 0.040 | 198 | 160 |
| 50k | 0.089 | 443 | 160 |
| 1M | 0.40 | 1 981 | 160 |
| 6.371M | 1 | 5 000 | 160 |

Why the square root: the planet's width is R / R⊕ of the real one's, its
heights s of them, so mountains are 1/s times steeper than real (49× on the
smallest planet, 11× at 50k, 2.5× at 1M, real at Earth's size). Proportional
heights would leave small planets flat and real heights would make them
needles. Land and sea have one scale.

**Detail by a spectrum, not a wavelength.** Between the grid's cells
(700 blocks apart at 50k, 89 km at Earth's) the column's detail is fractal
noise from a cell's wavelength down to 16 blocks, amplitude falling as
wavelength^0.8 (the roughness of real land), 150 m at 4 km, in the body's
metres (blocks / s), stronger in rising ranges and with the Roughness
setting. It still fades to nothing at the waterline.

**Volcanoes in metres**, scaled by s, no wider than a few cells.

## Paging

Already built: tiles, the pager, places as `{ body, cell }` (sprint 40),
ores and decorations seeded by place. What's left is play: a client walking
across tile edges on a large planet (handed over).

## Checks

- `TestScale`: relief grows with the radius as s does.
- The golden re-pinned (every world's heights change); TF9 stays (unreleased).
- `TestTerragen`, `TestFar`, `check:terragen`, `check:far`, `check:window`.

## Outcome

- `blocksPerMetre(code)` = √(R / R⊕) at most 1, `heightScale` = that ×
  5 000 (`METRES_PER_UNIT`) blocks per unit of elevation, land and sea alike
  (`world.h`); `LAND_SCALE` / `SEA_SCALE` are gone. Water levels,
  vegetation's slopes and volcanoes' bases use it.
- Detail (`World::surface`): fractal noise from a cell's wavelength (one
  wave a cell) down to 16 blocks, persistence 2^−0.8 (amplitude ∝
  wavelength^0.8), 1 500 m at 4 km in a rising range (the range of the
  noise; its typical swing is a third), a tenth of that on plains, × the
  Roughness setting, two fifths under the sea, fading at the waterline.
  Rising ranges (belt above its floor) are ridged: each octave folded at its
  middle, crests and V valleys. Each octave is read through one more turn
  of a fixed rotation, or the lattices' squares showed.
- Volcanoes in metres (arc cones 1.5–3.5 km, rift cones 0.5–1.3 km,
  shields 2–6 km and twelve heights wide), no wider than three cells or
  16 000 blocks (a cone is worked in fixed blocks).
- Bodies get the home's sky (a radius up, as high as the home's), not
  256 layers: a moon's highlands now stand higher than that.
- The create screen's size note gives the highest peaks in blocks instead
  of "bigger worlds are flatter".

| radius | heights along 40°N | was |
|---:|---:|---:|
| 2.6k | −43..105 | −33..38 |
| 50k | −180..284 | −33..38 |
| 1M | −770..1171 | −32..36 |
| 6.371M | −2017..2914 | −32..36 |

A column costs about 5.6 µs at every size (as before this sprint: the
smallest world reads fewer octaves than it did and costs the same).

**Paging:** built since sprints 39–40; `check:window` and `check:bodies`
pass with the taller skies. A client walking across tiles is handed over.

Golden re-pinned (812085123); TF9.

**Run:** `TestTerragen` 31/31, `TestScale` (relief now asserted to grow
with the radius, Earth's over ten times the smallest's), `TestBodies`,
`TestSphere`, `TestPlanetAtlas`, `TestFar`, `TestFluid`,
`TestPlanetWindow`, `bun check:terragen` 39/39, `check:far` 100%,
`check:window`, `check:bodies`, `check:lab`, UI typecheck. Not run: the
game.
