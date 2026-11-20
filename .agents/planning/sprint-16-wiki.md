# Sprint 16 — The wiki, and the sandbox engine

Goal: one place, in the menu and in game, that shows what everything is
made of and how it behaves, without telling the player what to do with it.
Its demos are tiny pieces of the universe run with the game's own physics:
no toy physics.

## Decisions

1. **The wiki** (`ui/src/wiki`): an icon rail (Matter, Reactions, Reactor,
   Lab), then the list, a live view and the facts, laid out like the old
   Matter screen. Sections link to each other: a reaction's reactants open
   in Matter, and a substance lists the reactions that make it and use it.
2. **The sandbox engine** (`src/sandbox`): a grid of up to 16³ blocks. Every
   block is a `matter::Mixture`, the same one the Reactor and furnaces use.
   - Reactions run in each block.
   - Heat conducts by each substance's conductivity in its state, and
     leaves the outer faces by convection and radiation.
   - Gases mix, hot gas rises, gas made by a reaction pushes out, gas used
     up draws more in (so a fire breathes), and the top is open to the
     room.
   - Liquids fall and level out.
   - Matter carries its heat when it moves.
3. **Changes of state take their time** (`matter::Mixture`). A substance
   crossing its melting or boiling point is split between the two states,
   and holds at that temperature until the heat to finish the change has
   arrived. Before this, ice melted all at once and then oscillated around
   0 °C. Matter arriving in another state resolves the same way (steam
   meeting cold water condenses and warms it).
4. **New data** in `matter/substances.json`: thermal conductivity for every
   substance (solid, liquid, gas). Densities are now parsed by the engine.
   Wood's charring rate is slowed to a real pace (minutes, not a twentieth
   of a second), which is what lets a fire burn instead of instantly
   charring.
5. **Demos come from the data.** Every reaction's demo is built from its
   own equation and numbers (`ui/src/wiki/demos.ts`):
   - reactants measured by the equation, with the gases around the solids;
   - the temperature where it runs at a watchable pace, kept where the
     liquids it needs stay liquid;
   - a spark for combustible gas mixtures.
   The Lab has hand-made physical demos: ice melting, water boiling, copper
   against wood, dry ice, tin pouring, fire in air against fire in
   nitrogen, and bronze.
6. **In game:** key **G** opens the wiki where the world allows it. That's
   a new Rules setting, "Knowledge: Wiki": on for Arcade, Classic and
   Hardy, off for Realistic. `tf_player` reads `tf_wiki` from `world.mt`
   and offers the screen through the UI channel.
7. **Fly and sprint:** Luanti never gives singleplayer `fly`, `fast` or
   `noclip`; games grant them. `tf_player` grants all three in creative
   worlds, and adds a real sprint (hold aux1, 1.6× speed) in every mode.

## Pieces

| Piece | Where |
|---|---|
| Sandbox: blocks, heat, gases, liquids | `src/sandbox/` |
| Gradual phase changes; `in`, `add`, `take` | `src/matter/mixture.*` |
| Densities and conductivities | `src/matter/matter.*`, `matter/substances.json` |
| Wiki verbs (`matter.*`, `sandbox.*`) for the whole session; stepping off the main thread | `src/shell/wiki.*`, `src/client/clientlauncher.*` |
| The G key and the wiki screen | `src/client/{keys.h,inputhandler.cpp,game.cpp}`, `src/shell/screens.cpp`, `builtin/settingtypes.txt` |
| Wiki page, sections, 3D sandbox view (three.js) | `ui/src/wiki/` |
| Rules: Knowledge / Wiki | `ui/src/menu/create/` |
| Fly, fast, noclip in creative; sprint; wiki offered | `games/terrafirma/mods/tf_player` |
| `bun check:wiki`: every demo run in the engine | `scripts/wiki-check*` |

---

## Outcome (2026-10-23)

All pieces landed. Verified:

- **409 engine tests pass.** New sandbox tests:
  - copper carries heat along a rod over 20× more than wood does;
  - ice melts and the water pours to the bottom;
  - wood burns in air (CO₂, and ~140 K hotter from its own fire) but in
    nitrogen only chars;
  - CO₂ made at the bottom of the box vents through the open top;
  - runs are deterministic;
  - bad sizes, overfull blocks and unknown substances are refused.
  New Mixture test: ice holds at 0 °C while part-melted, and water holds
  there while it freezes.
- **`bun check:wiki`: 35/35 demos run** in the engine sandbox for a
  simulated minute (27 reaction demos and 8 Lab demos), and every reaction
  demo makes its products. Getting there found five real problems, all fixed:
  - wood's charring rate (first fix below);
  - rusting's demo lacked water;
  - one temperature wasn't a number;
  - gas reactants didn't meet the ore they react with;
  - gas mixtures needed a spark.
- **`bun check:terragen`:** 37/37 on all four checks. **`bun check:lab`**
  passes.
- **UI** typechecks and builds. three.js 0.169 (MIT) is in its own chunk,
  loaded only when the wiki opens.

Not run: the client. Not seen by eye:
- the wiki itself (rail, sections, the 3D view, hover readouts, the heat
  overlay);
- G in game;
- flying in creative;
- sprinting.

Found and fixed along the way:
- **Wood's charring rate:** it was 20× too fast, so a block of wood turned
  to charcoal before any air could reach it.
- **The Mixture's all-at-once phase changes:** see decision 3.

Known gaps (honest):
- **No evaporation below boiling.** A substance's state is decided by its
  phase lines, with no partial pressures, so water doesn't evaporate below
  boiling and air holds no humidity in the sandbox.
- **Solids don't fall.** Powders and sand stay where they are put.
- **No power or light in the sandbox,** so electrolysis and photosynthesis
  show in the Reactor only.
- **The Reactor still re-creates its mixture each step.** It doesn't carry
  a part-finished melt between steps; moving it onto a one-block sandbox
  would.
- **Speed:** the busiest demo costs ~0.6 s of engine time per simulated
  minute, which caps how fast it can be run (stepping is off the main
  thread, so the game doesn't stutter).
