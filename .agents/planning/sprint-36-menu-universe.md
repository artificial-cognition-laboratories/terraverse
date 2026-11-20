# Sprint 36 — The menu is the universe

## Decision

Behind every menu page is the system of the world visited last (a fresh
install: the default world's), drawn by the Planetarium over the whole
window. New World doesn't open a box: the camera flies from the menu's
view to the world being made, and the settings are a panel along the side.

## Built (2026-10-28), for Cody to try

- `orbit.cpp` is gone. The window's planet host takes a backdrop
  (`shell::Planetarium::serve(body, backdrop)`): the menu serves the last
  visited world's code (`Worlds::lastVisited`), shown at once and whenever
  a page hides its planet. A page asking for the body already shown keeps
  it (nothing is made again).
- The menu's time runs at a day a minute: the planets move, Earth turns.
- Flights (`planetarium.fly { body?, latitude?, longitude?, altitude? or
  radii?, seconds }`): eased, the camera turning from the start's way out
  to the end's, looking at a point that goes from one body to the other,
  climbing to see both on a long hop. Input waits while it flies.
- The create screen fills the window: the rail and controls are a panel
  down the side, the planet the whole window behind (`usePlanetarium`
  `whole`). It flies in to the world being made (2.5 radii, 2.5 s); a body
  picked in the strip flies there; Home flies back out (4 radii).
- Fixed on the way: a world's last-opened time was read with a swallowed
  exception (`worldJson`); `Worlds::lastOpened` reads it once and throws on
  a value that isn't a time.

Then: the Dock (2026-10-28)

- `Dock` (kit): a panel floating down the left (rail, search, list or a
  workspace, footer) and a card floating on the right (`content`); the
  same props and slots as `WorkbenchPanel`, which stays for Settings,
  Community and the wiki. It reports what it covers (`covers`).
- Framing: `planetarium.frame { left, right, top, bottom }` (page pixels
  covered): every projection's middle moved to the free space's
  (`OrbitCamera::perspective`'s shift); clicks follow it. The universe
  still fills the window, so flights never jump.
- Play (`WorldSelect`) is a Dock: selecting a world shows its system and
  flies to it; its card is the world's details (the background is its
  planet, so the thumbnail went). Friends' and servers' worlds keep the
  menu's universe until they advertise a code.
- The creator is a Dock (wider for the detail pages); the body strip,
  views, overlays and date float over the free side.
- Fixed: "planetarium: needs 'x'". A request is spread into the message, so
  a viewport of `null` arrived as `{}`; the whole window is `{ whole: true }`.

Not yet: the page's other views (map, system, overlays) are still the
page's own, over the native planet; Adventures (presets, campaigns) as a
fourth tab; servers advertising their world code.
