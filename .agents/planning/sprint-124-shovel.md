# Sprint 124: the shovel

Earthworks by hand: flatten a site, dig a channel or a hole, heap a bank. Not precise, and
not meant to be: a straight edge is what timber framing filled with soil is for. The shovel
works loose ground (the `loose` group: soils, sand, gravel, clay, snow) at the size a shovel
moves, and the ground's own rules (piles: repose, yield) shape what's left.

## Verbs

**A tool works the world in its own cells.** Each tool has a bite, a cell size, and that's its
precision (a spade moves a spit, a trowel a handful). Aiming outlines the cell of the tool's grid
you're aiming into; holding digs it, and the next as you sweep. The world stays fine everywhere;
no tool works in metres. Round comes from the ground settling, not from the click. Each tool's
operations stay few; precision and size come from which tool you pick (more tools, thin menus).

| Shovel | A swing |
|---|---|
| Dig (a bore: `across` 16, a metre, out to `along` 80; quarter-block cells; digs.json) | a ring on the face where you look; each swing takes 8 quarter cells along your look, the nearest still there first: what's between you and the face, then the face chip by chip. Held, it opens a round hole you can walk into (Terraria's auto mode). It stops at what it can't move |
| Flatten (2 × 2) | the plane at your feet (Shift: the ground aimed at), outlined, dug from the top down while held |

- Nothing adds ground. Dug ground goes to the inventory as scoops, counted to the sixteenth.
- Learned (2026-11-24): a half-block cell to aim at is Minecraft, smaller and more finicky; smaller cells
  need less aiming, not more, so a swing takes a ball of them.

## Where it lives

- `assets/world/digs.json`: the shovel's item, what it moves (groups), the scoop size. Data, as
  cuts.json is.
- `server/game/dig/`: a leaf like `cut/`, booted from the table, answering `dig.choose`, `dig.take`.
- `shared/game/dig/`: where a scoop comes from and goes to, read by both (the client's ghost).
- `client/world/dig/`: the ghost and the asks, one call each from `game.cpp`.

## Decided

1. A scoop (the inventory's unit) is a block across, a quarter deep.
2. The tool's cell, dug by holding and sweeping (2026-11-24). Tried and dropped: forms (hole, trench,
   pit) outlined whole, needing setting up before acting (2026-11-24);
   click-then-confirm channels (2026-11-23); a blade-shaped bite a click, sculpted by hand, even with
   a locked floor and snapped facing (2026-11-24): no frame to see, so no control.
3. The shovel doesn't add ground for now.

## Next

- The operations in a strip over the held tool's slot, Alt+scroll to change; the radial goes.
- A smart mode (Terraria's): a rough sweep keeps to the row and depth it started on.
- More tools, each thin: trowel (fine cells, smooth), hoe (till, level), pick and chisel on stone;
  the axe trimmed, a saw for planks and beams.
- The pick: tunnel profiles (square, arched, round; 1×2, 2×2) advanced a bite a click into a face;
  looking down, a ramp or stairs.

- Spoil heaped beside the cut (a ridge, for beds), when placing ground comes back.
- The pick, then the axe's operations, by the same pattern.
