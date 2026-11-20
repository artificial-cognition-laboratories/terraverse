# Sprint 4 — the Lab

Goal: a place to see mechanics working. The main menu opens the **Lab**: a
neutral world laid out in small plots, one **scenario** each (a few blocks
that show one effect). In game, a Lab panel lists the scenarios, takes you to
one, resets it and controls time. Every mechanic from here on ships with its
scenarios, so it's visible the day it's built, and the same scenarios run
headless as checks. (Not the Axon labs; that integration is later.)

## Decisions

1. **The UI channel, game Lua ↔ page.** Needed by the Lab and by every
   system after it (body temperature next):
   - `player:set_ui(key, value)`: a Lua value sent as JSON to that player's
     page, which keeps the latest per key (`TOCLIENT_UI`);
   - `core.register_on_ui_event(function(player, name, payload))`: the page
     sends named events back (`TOSERVER_UI`, bridge verb `ui.send`).
   New packets mean a protocol bump (55); older peers are refused.
2. **Scenarios are data, declared by the mod that owns the mechanic**
   through `tf_lab` (a small, always-loaded game mod): id, title, group,
   description, a size, `build(area)` and an optional `expect` checked
   headless. Water's scenarios live in `tf_world`, the sky's in `tf_sky`.
3. **The Lab world is rebuilt fresh on every visit** at `<user>/lab`
   (outside the worlds list): `singlenode` with `tf_lab = true` in its map
   settings, which is what turns the Lab on. Plots sit in a row on a neutral
   floor; resetting a scenario clears its plot and builds it again.
4. **The panel is a screen** (key L), only in the Lab: scenario list by
   group, go, reset, and time speed (pause, ×1, ×10, ×100). A small strip in
   the HUD names the scenario you're standing in.
5. **`core.terra.enabled()`**: the Lab isn't a terra world, so systems that
   need terragen (the solar sky) ask first. Sky scenarios set the solar sky
   directly.
6. **`bun check:lab`**: a headless server builds every scenario, waits for
   each `expect`, and reports.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | UI channel: packets, `set_ui`, `register_on_ui_event` | `src/network`, `src/script`, `builtin/game` |
| 2 | Shell: UI values into the page, `ui.send`, `lab.enter`, the lab screen and key | `src/shell`, `src/client` |
| 3 | `tf_lab`: registry, floor, plots, reset, time, panel state | `games/terrafirma/mods/tf_lab` |
| 4 | First scenarios: water spilling from a broken basin, falling sand, the sky at the equator and the poles | `tf_world`, `tf_sky` |
| 5 | Page: Lab on the home screen, the Lab screen, the HUD strip | `ui/src` |
| 6 | Headless scenario check | `scripts/lab-check.sh` |

## Out of scope

Field overlays (they arrive with the first field that differs per block,
heat); schematics as scenario builds; loading scenarios fast enough for unit
tests; the Axon labs.

---

## Outcome (2026-10-22)

All six pieces landed. Verified:

- `bun check:lab`: both checked scenarios pass (water spills through the
  gap; sand falls and stone stays), and the five sky scenarios are listed as
  visual-only.
- The whole engine suite passes (375 tests). `TestTerragen` gains
  **lakes are flat**.
- `bun check:terragen`: 40/40 columns on terrain and water, 40/40 on
  `core.terra`. The determinism golden is unchanged.
- UI typechecks and builds. The gallery has a Lab page (`bun components`)
  where the panel and HUD strip answer the mock.

Not verified here: the client. That covers entering the Lab from the menu,
the L panel, go, reset and speed through the real channel, and the sky
scenarios by eye. The UI channel's round trip needs a connected client, so
nothing headless exercises it.

Found by the Lab on its first run:

- **The game's water was never a liquid.** `tf_world` registered water
  without `liquidtype`, so it never flowed anywhere, rivers included. That
  also means sprint 2's open question ("does water stay in its banks?") was
  never really tested. Water is a real liquid now.
- **With water flowing, lakes had steps.** The fill's jitter left one lake at
  22 blocks in one cell and 23 in the next, so the higher part spilled across
  the lake. Each lake now has one level, its outlet (`Grid::lake_level`), and
  a unit test holds it. Only lake surfaces change, and the preview and the
  golden don't, so codes stay TF2. Chunks generated before this, in existing
  worlds, keep the old lake levels; recreate a world for clean lakes.

Added beyond the plan: `core.set_time_speed` (a session override; setting
`time_speed` from Lua would have written to the player's own config).
Protocol 55 refuses stock Luanti peers in both directions.

Watch in the client: rivers now really flow. Where a river's surface steps
down between grid cells, expect small cascades. If its banks leak, water
will run off over the land. That's the real test of sprint 2's banks.

## Follow-up: the Lab browser (2026-10-22)

Clicking Lab now lands in a browser instead of on foot. Scenarios run down
the left with the live world behind them. Picking one glides the camera to
its plot and orbits it, so what you watch is the real engine, not a
recording. Reset and time speed work from there. **Step in** closes the
browser and hands you the camera at that plot; L reopens it.

- The camera is driven on the client: `set_ui("camera", { orbit = … })` →
  `shell::Director` → `Camera::setShot` (no wielded item while a shot is
  set). A server-driven camera would step at the server's rate.
- The page opens the browser on arrival (`screen.open`, a new session
  verb). Whether the panel is open decides the mode: open means browsing,
  closed means stepped in (`lab.browse` / `lab.step_in`).
- Scenarios may set their framing: `camera = { radius, height }`.

Verified: engine builds, `bun check:lab` passes, UI typechecks and builds,
and the gallery's Lab page walks through browse and step in. Not run: the
client (the orbit and glide by eye).
