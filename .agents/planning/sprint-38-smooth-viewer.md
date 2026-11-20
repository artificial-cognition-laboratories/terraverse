# Sprint 38 — A smooth viewer

## Decision

Nothing the player does in the viewer stalls a frame: measure first, fix
what the numbers show, and keep a benchmark that says so. Then the controls
a viewer needs: the region seams on and off, and one clock to stop, run and
scrub.

## Measured (headless, `luanti --planetarium "" --planetarium-bench 18`)

| Moment                          | Before | After |
|---------------------------------|-------:|------:|
| Switching a layer               | 171 ms |  8 ms |
| A new world (a changed setting) | 214 ms | 11 ms |
| Dropping a world still building |  96 ms |  3 ms |
| An ordinary frame               | 2–3 ms | 2–3 ms |

The bench prints its worst frames and what a new world costs the asking
thread; the Planetarium logs its worst frame (and the tiles' share) every
120 frames (`--info`).

## Built (2026-10-28)

- Layers sweep in: `Tiles::tint` marks the tiles out of date and
  `update()` repaints them within 3 ms a frame, the drawn ones first
  (`unpainted()`: how many are left). It used to recolour every held tile
  in one frame.
- A new world doesn't wait on its model: the tile workers built the world
  model holding their queue's lock, so the main thread's first `want()`
  waited for it. One worker claims the build and makes it outside the lock.
- A dropped world isn't waited for: the workers share their state
  (`Crew`: copies of what they build from, the queue, the results), so a
  world dropped mid-build stops them and doesn't join them; the last one
  out frees it. (The in-game far view gets all three.)
- The menu lays out each world size's atlas once (40 ms each; a changed
  seed or setting keeps the size).
- One clock, the host's (`planetarium.time { running?, date? }`, the date a
  share of the year): the menu ticks it; stopped, the planets and the
  Earth's spin stop. The page's slider scrubs it (and stops it there; the
  climate preview follows the date only then, not four times a second);
  while it runs the host reports the date (`event: "time"`) and the slider
  follows. In game the world's own date drives it, as before.
- Region seams on and off (`planetarium.seams { shown }`): a tool button.
- The page paints each overlay once a world (switching back is free).

Open:

- A detached tile worker still building when the game quits may run on
  through static destruction; it reads only its own copies, but it's worth
  watching for a crash at exit.
- The page's side of a changed setting (its preview, the bodies' looks) has
  no timings yet; if a stall remains, that's where to look.
