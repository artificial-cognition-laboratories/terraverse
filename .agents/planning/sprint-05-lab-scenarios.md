# Sprint 5 — Lab scenarios: one world each, with controls

Goal: the Lab is a page like the world creator. Scenarios are listed on the
left; the right is a live view of the chosen scenario's **own tiny world**,
with its controls around it (sliders, toggles, choices, buttons) plus restart,
time speed, step in and back. Choosing another scenario swaps the world under
the same page.

## Decisions

1. **One world per scenario**, built fresh at `<user>/lab` when chosen: no
   interference, each with its own settings, quick to build.
2. **Scenarios are data plus behaviour.** Each mod's `lab.json` declares its
   scenarios: id, group, title, description, size, world settings and
   **controls**. The menu reads it before any game Lua runs. The mod's Lua
   supplies behaviour by id (`tf_lab.behaviour(id, { build, control, enter,
   expect })`). Loading fails if the JSON and the Lua disagree.
3. **Controls:** `button`, `toggle`, `slider`, `choice`, each with an id,
   label and range or options. The page renders them with kit components;
   changes go to Lua (`lab.control { id, value }`), which keeps the values
   and sends them back with the scenario state. Restart, time and step in are
   always there.
4. **The page outlives the world.** The Lab view is page state, shown in the
   menu (list, viewport waiting) and in game (viewport live). Switching
   scenario: the page leaves the current world and enters the next when the
   menu returns (see the outcome).
5. **The viewport is the real game view**, framed by the page; the camera
   orbits the plot (sprint 4's director). Snapshots for the list come later.
6. **`bun check:lab`** runs one headless world per scenario.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | `lab.json` format; shell index (`lab.list`), `lab.enter { id }` | `src/shell` |
| 2 | `tf_lab`: JSON + behaviours, one-scenario world, controls, check mode | `games/terrafirma/mods/tf_lab` |
| 3 | Scenarios rewritten: water (gap width), falling (material, drop), the sun (latitude, day of year) | `tf_world`, `tf_sky` |
| 4 | Page: the Lab view across menu and game, controls | `ui/src/lab` |
| 5 | Per-scenario headless check | `scripts/lab-check.sh` |

---

## Outcome (2026-10-22)

All five pieces landed. Verified:

- `bun check:lab`: every scenario in its own headless world. Water (with
  the default gap) and falling blocks pass; the sun is listed as visual.
- Scenario worlds boot headless without errors on the path the client
  takes.
- `bun check:terragen`: 40/40 and 40/40 (normal worlds still load with the
  reworked `tf_lab`). The engine builds; UI typechecks and builds. The
  gallery's Lab page walks the whole flow against a mock: list, choose, the
  world "loading", controls answering, and Stop.

Not run: the client. That covers loading a scenario from the Lab page,
switching between scenarios, the controls changing the world, and the orbit
inside the page's frame.

Changed from the plan: switching scenarios needs no engine state. The page
outlives the worlds, so it leaves the current one and asks for the next
when the menu returns (`ui/src/lab/flow.ts`).

Fixed along the way, from Cody's log:
- **World settings never applied.** `creative_mode` and `enable_damage` in
  `world.mt` had no effect: Luanti's own Lua menu used to copy them into
  the engine's settings when you picked a world, and our menu skips it. Now
  `Worlds::select` does the same. That's why you could take damage in the
  Lab.
- **"No handler for ui.send" on leaving.** As the session ended, the screen
  closing was announced to the page after the session's verbs were gone,
  and the page answered it. The session's teardown no longer announces;
  the page drops its screen with the mode change.

The Lab's first scenarios: water (gap width), falling blocks (sand or
gravel, drop again), and the sun's path (latitude, day after the spring
equinox, axial tilt, noon, midnight). The five sky scenarios from sprint 4
are now one.
