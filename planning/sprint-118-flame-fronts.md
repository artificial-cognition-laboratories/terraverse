# Sprint 118: Flame fronts

Phase 2 of `gas-and-fire.md`. Before this, a premixed gas burned as soon as it
was hot, all of it in one step: methane went in a frame.

## Built

- **A gas fuel's flame is data.** Each gas combustion in `reactions.json` has a
  `front`: its fuel, its laminar flame speed at its fastest mixture, that
  mixture (an equivalence ratio), its lean and rich limits, and the temperature
  it autoignites at. Methane 0.38 m/s, φ 0.5–1.7, 810 K; hydrogen 3.0 m/s,
  φ 0.1–7.1; carbon monoxide, propane, ethanol and octane likewise. Limits are by
  equivalence ratio, so they hold in any atmosphere, not only Earth's air.
- **A block burns no faster than its front crosses it** (`Mixture::frontRate`):
  its fuel goes in across/speed. Speed falls to nothing at either limit, rises
  with warmth (T^1.75, up to 600 K: a burning block's own heat is behind its
  front) and with stirring (a fifth of its wind's speed). Only the gas: a pool of
  spirit burns only what it gives off.
- **Fronts travel** (`WorldMatter`, `Cell::front`). A block lit (a spark, a hot
  wall, a flame beside it) keeps how far its front has crossed it, 0 to 1, at its
  mixture's speed. While lit it burns at the front's pace whatever its average
  temperature: the flame's thin zone is hot however cold the rest is. Across, it
  lights its neighbours that can carry a flame. Saved (world matter version 6).
- **Outside its limits a mixture can't carry a flame**, but held above its
  autoignition temperature by something else it burns slowly (a tenth of its
  flame speed): a torch over a spirit pool's thin vapour.
- **Wiki:** "A gallery of firedamp" (lit at one end, the front runs its length)
  and "Too lean to burn", under Flames in gas, each checked by what methane is
  left.

## What it does

A sealed ten-metre tube of stoichiometric methane, lit at one end, burns block by
block over 4.8 s: about 1.9 m/s. A real closed tube lit at its closed end runs at
the laminar speed times the burnt gas's expansion (about 7), some 2.6 m/s, and
here it's the winds of phase 1 carrying the burnt gas's swell that do it. At a
third the methane the spark burns its own block and nothing else.

## Found on the way

- **Overpressure no longer cascades.** A windy block over 25% used to even out
  into its awake neighbours in one pass, block after block: a spark's expansion
  ran a whole tube at once and piled the gas against its far end. Now a windy
  block over that vents only into open air beside it; enclosed, the wind
  carries it.
- **Falling liquid leaves a trail awake.** Condensate raining off the edge of a
  test's floating slab fell to y = −827, waking every block on its way (8,000 of
  them). The tests now stand on ground; the trail itself is still there, for a
  waterfall off a cliff. Worth its own job.

## Not doing (yet)

- Pressure's effect on flame speed (∝ p^−0.3 to −0.5).
- Fuel pushed sideways out of a burning block still goes unburnt into the room
  (sprint 117); the front only crosses into awake blocks.
- Detonation, and fronts turning to blasts: phase 4.
