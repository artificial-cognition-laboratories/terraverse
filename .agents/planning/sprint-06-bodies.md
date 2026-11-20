# Sprint 6 — the body model and the System tab

Goal: every body in the system is worked out by one model in terragen from
its causes, and the create screen's System tab is rebuilt on it: causes on
the left, what each body became and how, from the engine. Step 1 of
`planning/systems-and-bodies.md`.

## Decisions

1. **A world is a short history.** `terragen::bodies(star, causes)` steps
   each body from formation to the system's age in 0.05 Gyr steps:
   interior heat (radioactive, leftover, tidal), the core and its field,
   outgassing, thermal escape, the stellar wind where no field shields, a
   giant's magnetosphere for its close moons, oceans, weathering, plates,
   life and oxygen. Today's **card** and the **history** (lasting changes;
   a state that keeps flipping is told once) fall out.
2. **Causes only on the page:** star (type, age, wind); per body: parent,
   orbit, eccentricity, inclination, mass, where it formed (composition
   follows: rock and metal inside the frost line, ice beyond, giants from
   big bodies beyond it), the volatiles it formed with, day, tilt, life.
   No outcome is a control.
3. **One implementation:** fixed point in terragen (new `log2`, `exp2`,
   `pow`, `exp`), served by `system.preview`. The page computes nothing
   about a body; the gallery mock answers with canned cards.
4. **Calibrated on our solar system:** Earth, Venus, Mars, Mercury, the
   Moon, Jupiter, Io, Europa, Saturn and Titan, from their real causes,
   must each come out as themselves. A unit test holds it.
5. **One body model on the page** (`ui/src/system/`), replacing the two in
   the first sketch. The playable world's tilt and year stay in the world
   code (World tab); everything else about it is on System.

## Outcome (2026-10-22)

All landed. Verified:

- `TestTerragen`: 18 tests pass. New: fixed-point log/exp (exact at powers
  of two, round trips across the range, saturation), and **our solar
  system from causes**: every body's kind is right. Earth comes out at
  0.7–1.6 bar of N₂/O₂, 5–27 °C, with a field, oceans and plates. Venus
  has thick CO₂ and a runaway in its story; Mercury doesn't. Mars's story
  has its field fading and its oceans freezing. The Moon is locked and Io
  tidally heated. The same causes give the same cards, and impossible
  systems are refused. The whole engine suite passes (377 tests).
- `system.preview`'s own code, run standalone on the page's starter
  system: Cinder airless, Earth temperate (1.3 bar, 17 °C), Vesper a thin
  cold desert, Jovian a giant, Pyre volcanic, Nacre and Hale ice shells.
- UI typechecks and builds.

Not run: the client (the System tab against the live engine, the sticky
card while dragging, the globes painted from cards).

Fixed from the first sketch: two body models became one; family chosen by
id became an outcome; activity, core convection and atmosphere sliders
became outcomes; derived values computed in the component moved to
terragen; Earth's tilt is stored once.

Known: the model is calibrated, not physical in detail. Europa keeps more
air than it should (its outgassing is volcanic rather than through ice).
Composition comes from one "where it formed" slider, with no fine-tune
yet. Nothing about other bodies is saved with a world (that's the system
code, later).
