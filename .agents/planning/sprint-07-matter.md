# Sprint 7 — Matter: the building blocks

Goal: the engine knows its building blocks. Elements and substances are
engine data; given a temperature and pressure, the engine says what state
a substance is in. A **Matter** screen (from a new workshop icon row on
the home screen, next to the Lab) shows them: a periodic table, each
substance's structure in 3D, and its phase diagram to drag across.

## Decisions

1. **Data, not code:** `matter/elements.json` (all 118 elements' identity:
   number, symbol, name, mass, category, grid place, colour, radius) and
   `matter/substances.json` (the modelled substances). The engine knows
   what a triple point is; the files say what water's is. Games can add
   to them later.
2. **Phase from a few anchors:** the triple point, the critical point, and
   either the boiling point at 1 atm or the sublimation point at 1 atm.
   The vapour curve runs through them, straight in ln p against 1/T between
   anchors (Clausius–Clapeyron); the sublimation curve runs through the 1 atm
   point or from the latent heats; the melting line is straight from the
   triple point with its slope (negative for water and silicon), held at
   half the triple temperature. Exact at the anchors; approximate between.
3. **One implementation:** `src/matter` (doubles: runtime physics, not
   world generation), served by `matter.list` (tables plus each substance's
   curves, sampled) and `matter.state`. The page draws the regions from the
   engine's curves and never decides a state itself.
4. **The workshop:** an icon row bottom right of the home screen (Lab,
   Matter), replacing the Lab button.

## Outcome (2026-10-23)

Landed. 20 substances: water, CO₂, methane, ammonia, nitrogen, oxygen,
hydrogen, SO₂, sulfuric acid, iron, copper, tin, gold, aluminium, lead,
mercury, silicon, carbon (graphite), salt, silica. Verified:

- `TestMatter` (4 tests): all 118 elements load; molar masses come from
  compositions. Water is solid, liquid, gas and supercritical where it
  should be; it boils at 100 °C at 1 atm and cooler on a mountain; squeezed
  ice melts below 0 °C. Methane is liquid on Titan while nitrogen stays air.
  Dry ice sublimes at 1 atm and is liquid at 10 bar. CO₂ at Venus's surface
  is supercritical. Mercury is liquid at room temperature. Tin melts in a
  campfire and copper doesn't. Every curve meets the triple point and
  passes its anchors, and no melting line runs below absolute zero. The
  whole engine suite passes (381 tests).
- UI typechecks and builds.

Not run: the client (the Matter screen against the engine, the 3D view,
dragging the diagram).

Fixed while testing: the classifier missed liquid below the triple-point
temperature under pressure (water's leaning melting line), and latent
heats alone put CO₂'s sublimation point a few kelvin off, hence the 1 atm
sublimation anchor.

Known: helium isn't modelled (no ordinary triple point). Metals' critical
points are estimates (marked). There are no mixtures or reactions yet; the
reference worlds on the diagram are fixed numbers, not yet the system's
cards. Structures are drawn from simple lattices.
