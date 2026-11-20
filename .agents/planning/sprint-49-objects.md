# Sprint 49 — Objects

## Decision

Ghost isn't a flag (sprint 48's was the wrong model): an object is its
design and what's built of it. What's designed and not built is the ghost
(survival: the work left); in creative what's built follows the design.

- **X**: build mode, the world around you. Terrain isn't focused any more.
- **N** (tools out, or the rail): a new object, one block of the block
  held, held up in front of you, and the focus on it.
- **F** on an object: picked up: it lifts half a node, held still; the
  camera orbits it, the tools edit its design. **Q/E** turn it 15° (Shift:
  90°). **F/Esc**: let go: it falls till it rests.
- Library: a blueprint keeps the design; placed in creative it's built.

## Done (2026-10-29)

- `piece::Object` (TFO1: id, design, built); `Piece` is a grid again.
  TFP1/TFP2 entities read as objects built as designed. `unbuilt()`: the
  difference, to the sixteenth; drawn see-through after the built mesh.
- `ObjectProperties::validate`: stood on as far as built, selected as far
  as designed; nothing built, passed through.
- Workshop: `start` (in front, held; TOCLIENT_HELD tells the client, which
  focuses it when the entity arrives), `hold`, `turn`; strokes on the
  design then `build()`. Gravity from `movement_gravity` (the entity on
  activate, and let go).
- Keys: N (hotbar-next is the gamepad's only now), Q/E while held.
  Protocol 68. Make real is gone from the rail.

Proved: TestPiece `testAnObject` (id, renew, unbuilt to the sixteenth,
legacy texts), `testSentWithItsEntity` (boxes, passed through unbuilt):
fail sabotaged. Headless server boots on Cody's latest world.

Not proved / known:
- None of the flow seen: lift, fall, turn, N's focus.
- Collision boxes aren't turned with the object (Luanti's are axis-aligned).
- A player who leaves holding an object leaves it floating till it's next
  loaded (the workshop doesn't hear of leaving).
- Nothing's ever unbuilt in creative, so the ghost diff draws nothing yet.
