# Sprint 3 — the sun and seasons on the live world

Goal: the world keeps a calendar, the sky follows the real sun for where you
stand and what day it is (short winter days, midnight sun near the poles, the
noon sun low in the south up north), and game Lua can ask terragen for the
date, the sun and the air temperature at any point. This is the base the
seasonal systems (body temperature first) stand on. Phase 3 of AGENTS.md
"Generation" / "The sun".

## Decisions

1. **The calendar is a pure function of the engine's clock.**
   `date = frac((day_count + time_of_day) / year_days)`; day 0 is the northern
   spring equinox, as in the preview. No new saved state — the engine already
   saves the day count and the time of day.
2. **Seasons are local.** The season name depends on the hemisphere (summer in
   the north is winter in the south); terragen owns `season(date, latitude)`.
3. **One solar model, three consumers.** terragen gains the sun's position
   (elevation for latitude, declination, hour) and day length in fixed point,
   next to the existing insolation. Gameplay reads terragen (deterministic,
   server-side). The client sky is visual only and uses floats with the same
   formulas.
4. **Air temperature in °C** comes from the column's climate *on that date*
   (`World::column` with that day's sunlight — the preview's model) plus a
   daily swing that peaks mid-afternoon and is larger where it's dry. The
   scale is fitted to the existing biome thresholds: snow below ≈ −10 °C,
   taiga/tundra below ≈ 0 °C, jungle and desert above ≈ 24 °C.
5. **Lua API `core.terra`** (engine, backed by terragen, server only):
   `world()`, `calendar()`, `sun(pos)`, `climate(pos)`. On a world that isn't
   `terra` every call raises an error — no fallback.
6. **The sky follows the real sun.** The engine's sky places the sun on a
   circle tilted by the orbit tilt; that circle is exactly the sun's path at a
   latitude at equinox (tilt = −latitude). A declination shifts the circle
   toward the pole. So the sky gains one parameter, `solar = { latitude,
   declination }`, in `set_sky`:
   - the sun, the moon (opposite) and the stars (about the celestial pole)
     are placed from the real time of day, the latitude and the declination;
   - everything that colours the sky and lights the world (sky colours,
     sunrise glow, day/night light ratio, star fade) is fed the **apparent
     time**: the stock time of day at which the stock sun would stand at the
     real sun's height. Polar night stays dark; the midnight sun stays light.
     The sunrise glow sits at the sun's bearing.
   - sent as a trailing field of `TOCLIENT_SET_SKY` (older peers ignore it).
7. **`tf_sky` mod** sets each player's solar sky from `core.terra.sun(pos)`
   and refreshes it as the date or their latitude moves.

## Work

| # | Piece | Where |
|---|---|---|
| 1 | Sun position, day length, season; calendar; air temperature | `src/terragen/sun.*`, `world.*` |
| 2 | Unit tests: noon sun height, polar night/day, day length, hemispheres, calendar, temperature sanity | `src/unittest/test_terragen.cpp` |
| 3 | `core.terra` Lua API | `src/script/lua_api/l_terra.*` |
| 4 | Sky params + packet + `set_sky{solar}` / `get_sky` | `skyparams.h`, `server.cpp`, `clientpackethandler.cpp`, `l_object.cpp` |
| 5 | Solar sky: body placement, apparent time, light ratio | `src/client/sky.*`, `clientenvironment.*`, `game.cpp` |
| 6 | `tf_sky` mod | `games/terrafirma/mods/tf_sky` |
| 7 | Headless check: `core.terra` answers match standalone terragen | `scripts/terragen-check*` |

## Verification

Unit tests; the whole engine suite; `bun check:terragen` extended to
`core.terra`; client builds. Not run: the client (the sky by eye).

## Out of scope

Seasonal surface (snow cover, leaves, frozen water), body temperature (next
sprint), weather, the moon's own orbit and phases, a date/season readout in
the HUD (needs the planned `player:set_ui` channel), longitude-dependent time.

---

## Outcome (2026-10-22)

All seven pieces landed. Verified:

- `TestTerragen`: 15 tests pass. New: the sun path (noon height at the
  equator and at 60°, sunrise at six at equinox, polar night, midnight sun,
  the south mirroring the north), day length, the calendar (year/day/date,
  never reaching the next year early), seasons per hemisphere, and
  temperature (summer warmer than winter in both hemispheres, afternoon
  warmer than night, equator warmer than 67°N, sane range). The determinism
  golden is unchanged: generation didn't move.
- `TestSolar` (client): 3 tests. The float sun matches terragen's heights;
  the wicked-time inverse round-trips; the light ratio is full at noon, night
  at midnight, a twilight noon just inside the Arctic, dawn-bright under a low
  midnight sun, and equal for equal sun heights either side of noon.
- The whole engine suite passes (374 tests).
- `bun check:terragen`: 40/40 columns agree on terrain and water, and
  **40/40 agree on `core.terra`**: calendar, sun and climate answers from a
  live headless server match standalone terragen for the clock the server
  read.

Not verified here: the client by eye (the sun's path, polar light, the glow
at the sun's bearing, shadows), and `set_sky{solar}` from a connected player.
The headless check has no players, so `tf_sky`'s sends aren't exercised.

Found along the way:

- `core.terra` reads the code from the map settings, not the map generator:
  mods load before the generator exists. The first version crashed on load
  because of that.
- Luanti's `setfloatfield` narrows to float. `core.terra` pushes doubles, so
  Lua sees terragen's values exactly.
- The stock light curve has no light below about −6° sun height. A noon sun
  at −13° (80°N in deep winter) is full night, with no twilight band. That's
  fine for now; a real twilight is a sky-colour change for later.
- `tf_sky` makes a world terra didn't generate refuse to load, with
  `core.terra`'s message, the same way TF1 worlds are refused.
