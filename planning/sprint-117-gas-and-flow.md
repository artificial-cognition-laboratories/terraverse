# Sprint 117: Gas with momentum, liquids by their viscosity

Phase 1 of `gas-and-fire.md`: cells, viscosity and the open face.

## Built

- **Viscosity is a substance's.** Every substance in `substances.json` has its
  liquid's viscosity at a temperature and an activation energy (Andrade's law),
  so it thins as it warms: water 0.89 mPa·s, molten glass tens to thousands of
  Pa·s, silica millions. A block's is the log-mean of its liquids by moles
  (`Physics::runniness`, m²/s).
- **Liquids feel it.** A virtual pipe's flow is held back by 3νL/d³, implicitly,
  on top of the old arcade damping: the drag at which a pipe settles to what a
  sheet of depth d carries in slow flow, g·Δ·d³/(3ν). (3ν/d² alone, the sheet's
  own rate, let honey level a mould twenty times too fast: a pipe's face is L²,
  a sheet's L·d.) Water is unchanged. A metre of anything still slumps; a thin
  edge of glass creeps.
- **Gas has momentum** (`sandbox/wind.cpp`). Each face of an awake block that
  isn't mostly solid carries a wind, kept between steps and saved (world matter
  version 5). It speeds up with the difference in pressure across the face and
  with buoyancy, and drag slows it. Gas moves with it, carrying its heat.
- **The atmosphere is a reservoir.** A face onto asleep air pushes against the
  body's pressure and the weather's temperature. Gas leaving is gone; air drawn
  in is the room's. Smoke and warm air leaving along their buoyancy wake the air
  they go into, so a plume is as tall as it stays warm. A column moving past open
  air entrains it (α 0.1), which is what stops a plume rising forever.
- **Counterflow stays.** One face carries one net wind, so warm gas under cool
  still trades places within a face (the old exchange): a fire in a pit draws
  fresh air down its edges while smoke rises up the middle.
- **Gas is clear.** Radiation lands only where there's a surface (1% of a block
  solid or liquid). Hot air no longer heats the air beside it by shining on it.

## Choices

- **Pressure waves travel at 10 m/s, not 340.** Low-Mach: slow enough to step at
  25 Hz, fast against anything a player sees air do. The cost: a steady column
  holds pressure differences of a few percent (ρgh scaled by (340/10)²). Blasts
  get their own treatment (phase 4) and don't come through this.
- **A block over 25% overpressure still just vents** (the old balance), so a
  reaction that makes gas in one chemistry step can't ring the winds.
- **Fuel gas pushed out sideways goes unburnt.** Following it everywhere made a
  fireball of 1,400 blocks: wood gas burns too slowly in metre blocks to stay
  near the log. Carrying a flame sideways is phase 2's flame front.
- Solids' pores and vacuums keep the old exchange: no wind.

## Found on the way

- The world fire test passed only on a one-second lighting spike (1680 K). A
  buried log, charring, runs as hot as an open fire's steady burn (~1100 K): wood
  charring gives off heat of its own. The test now says what air changes: an
  open fire burns its charcoal away (9,873 mol left of 11,100, against 10,612
  before winds: it burns better, and hotter, 1117 K steady against 1068 K).
- Energy summed from `activity` (heat given off) can't measure a fire: CO and CO₂
  cycling between blocks count over and over (21.8 GJ "released" by a 4 GJ log).

## Cost

Wind blocks cost about 30 µs a step each (25 substeps), several times a solid
block. A fire now wakes ~12 blocks (19 before), so the fire test runs faster
(4.4 s, was 7.1 s); a 2 MW steam plume over wet soil wakes ~210 (4.2 s, was 0.3).

## Next (phase 2)

Flame fronts: a fire spreads to a neighbour at its mixture's flame speed and
burns no faster than oxygen mixes in.
