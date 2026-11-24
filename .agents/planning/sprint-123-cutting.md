# Sprint 123: cutting

The first slice of making with members (`making-and-labour.md`, "Members and joining"): an axe,
a log, and members peeled off it. Creative first; survival's tool-making comes after.

## The flow to test

1. Hold an axe, look at a log, press **F**: a radial menu opens over the game, glass as the
   hotbar's. Inner ring: what to cut (beam, plank, post, firewood); outer: its size.
2. Choose: the menu closes, the axe keeps that cut until another is chosen.
3. Looking at a log, the member that would come off shows on it, transparent.
4. **Left click**: it comes off at once (no wait, for now) into the inventory, and the log loses
   exactly that volume. What's left keeps being cut until it's gone.
5. Everything else stays as it is: F on a machine still focuses it; the axe digs anything else
   as a tool digs; mining stays whole blocks.

## Steps

1. **An axe.** `assets/world/tools.json`, read as `hand.json` is (`server/game/registry.cpp`):
   `core:axe`, a tool, in the creative catalogue. TEMP: a placeholder face until the asset tools
   draw one.
2. **The cuts table.** `assets/world/cuts.json`: which tools cut which blocks (logs, by group),
   each cut a form and its standard sizes as a box in sixteenths, and what the product is.
   Products are registered as items per form × size × wood (`core:beam_2m_birch`), their weight
   from the wood's matter. One table read by everything; nothing per item in code.
3. **F, routed.** Client: F with a cutting tool held, pointed at a block it cuts, opens the
   radial (`screen.open cut`) instead of the focus view. Its own file in `client/`, one call from
   `game.cpp`.
4. **The radial.** `game/cut/` in the page, a kit `Radial` component: two rings, icons, the
   choice sent as a UI event and kept on the server per player and tool (a leaf in
   `server/game/cut/`), and echoed to the HUD so the client can preview it.
5. **The preview.** The chosen member's box drawn on the pointed log, against the face looked at.
6. **The cut.** Server, on dig: an axe with a cut chosen, on a block it cuts, takes the member's
   box out of the block (the block's shape at sixteenths, as sculpt cuts), gives the product, and
   removes the block when nothing's left. Refused, with why, when the box doesn't fit what's left.

## Not this sprint

Grades by tool (one grade: hewn), fasteners and assembly, procedural trees' trunks, the
shovel's terrain operations (flatten, dig to a floor, keep the slope) and placing operations:
the same language, next.

## Checks

Engine build, UI typecheck, a headless boot (the new tables load, an unknown field refuses), and
the flow by hand in creative: an axe from the catalogue, a birch log, F, a plank, the log
shrinking to nothing.

## Round two (2026-11-23, after the first play)

Played: harvesting planks, beams, posts and firewood off a log works and feels right. Then built
(not yet played):
the block blinking out (the client now asks the server for the cut, `cut.take`, in place of a dig,
and predicts nothing); slivers left between members (every size divides a block, the box lands on
its own grid where it's aimed, and a face with nothing whole left gives offcuts); cutting where
the crosshair is; members of 2 m and 4 m from a log of several blocks lying along one grain
(`peel::read`, at most four). `TestPeel` pins the rule.

**Structure is world, motion is machine.** A member placed (right-click, a ghost of its box
where it'll go; sneaking stands it up) is its wood's timber written into the world at
sixteenths (`core:timber_<wood>`, grain kept): it stacks, makes frames and floors, saves and
streams as any block, and is cut again by the axe as a log is; dug bare-handed it's offcuts.
It's a machine only when made one: built in workpiece mode, or (later) a joined group lifted
out of the world. Members of one wood share a block; another wood in the same block is refused.

Next: fasteners (pegs, cord) joining members where they touch; discs cut across the grain as the
first wheel; firewood worked to kindling in the grid; carrying by weight.
