# Sprint 33 — The Planetarium in the game

## Decision

The Planetarium (sprint 31, 32) becomes what the player sees of the planet:
the in-game map and the create screen. The page draws its panels; the
native Planetarium draws the planet under them. This replaces `MapGlobe.vue`
in game and `globe.ts` on the create screen. The menu background
(`orbit.cpp`) and sprint 32's cutaway and overlays come after.

## Phases

### 1. The bridge

- `planetarium::Host` (native): the planet a window shows, if any. A
  manager: `show` swaps the body in (the old one stays on screen until the
  new one's coarse layer is built: no blank planet), `hide`, and the
  verbs dispatch through its current Planetarium. It takes the input where
  the page doesn't, and draws under the page.
- Picking: a click (not a drag) is a ray from the camera onto the body: the
  place (latitude, longitude) and the atlas cell there.
- Layers: markers (the player), with the seams as before.
- The contract (`ui/src/bridge/types.ts`), validated where it arrives:
  `planetarium.show { body?, focus?, markers? }`, `planetarium.hide`,
  `planetarium.focus { lat, lon, altitude? }`, `planetarium.markers`;
  back: `planetarium.picked { lat, lon, cell }`.

### 2. The in-game map

- M's map screen: the planet under it is this world's (its atlas, its land
  from the live content), focused on the player, their marker on it.
- Teleport (creative): click a place, then Go there. The page asks the
  server (`ui.send map.teleport { lat, lon }`); the server checks the
  privilege, finds the ground there through the atlas (as spawn does), and
  moves the player. The server decides, never the page.
- The flat map and its overlays stay the page's for now.

### 3. The create screen

- The planet behind the create screen is the world being made: its code,
  its size (the atlas's radius), from the baked palette.
- A changed seed or setting shows the new planet; the old one stays until
  the new one's coarse layer is ready. Measured: how long a world model
  takes to build for a new code.

## Built (2026-10-28), for Cody to try

- `shell::Planetarium` (`shell/planetarium.h`): the host. It lives in the
  shell (it's a page contract, and the shell builds as C++20 for CEF; the
  renderer, `planetarium/`, stays free of the page). Verbs
  `planetarium.show / hide / focus / markers / viewport`, checked where they
  arrive; `picked` sent back. Swaps keep the old body until the new one's
  coarse layer is built, and carry the camera over.
- The renderer draws into a rectangle (the page's `viewport`, page pixels ×
  the shell's scale) and takes only mouse input that starts inside it.
  Picking: a click is a ray onto the sea's sphere (`OrbitCamera::ray`,
  tested), giving the place and the atlas cell. Markers (`MarkerLayer`): a
  mast and a ring, hidden past the horizon.
- In game: `Session` owns the Planetarium, its body always this world (the
  atlas, the horizon, the live palette). F7 shows it alone (its key and Esc
  close it, and it has the keyboard); under a screen the page shows and
  hides it, and keys go on to the screen. `Game` draws it instead of the
  world while it's shown.
- The map screen's globe view is the native planet, the player marked,
  Find me focusing on them. Creative: click a place, Go there. tf_player's
  `map.teleport` checks the mode and the place, finds the ground as spawn
  does, and moves the player (it logs and refuses anything else).
  `MapGlobe.vue` is gone; the flat map and its overlays stay the page's.
  The map's flight-trajectory polling went with the flight pack.
- The create screen: the playable world's terrain globe is the native
  planet, made from the draft's code with the baked palette (`Menu` hosts
  it; `far::Baked::load` is the one loader); a new seed swaps it in place.
  The overlays (biomes, temperature, …), the inside cutaway and concept
  bodies still use the page's globe, so `globe.ts` stays until those are
  native layers.
- `usePlanetarium` (ui/src/planetarium/use.ts): one composable for both
  screens: show while active, follow the element, swap on a new body, keep
  markers, hide on unmount. Behind it the page's backgrounds go
  transparent (`:has(.native)`).

Not measured yet: how long a new code's world model takes to build (the
swap hides it, but a slow one shows the old planet for that long).

## Outcome

(filled in at the end)
