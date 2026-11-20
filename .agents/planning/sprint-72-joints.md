# Sprint 72 — Joints and the floor

## Done

- **F and Tab are separate.** F only puts an object on the workbench and
  takes it off; build mode is Tab's alone. Esc no longer drops the workpiece
  (it closes a menu, cancels a stroke, unselects a part, else pauses).
- **Delete is safer.** It isn't offered on an object's only part (that
  deleted the whole object), and it takes a second click ("sure? click").
- **Joints are picked by looking at them.** With the tools away and an
  object on the bench, every joint and socket has a small blue marker; the
  one nearest the look is drawn larger and lit. Right-click it for its own
  menu: fixed, rotary, hinge (hovered: shown where it'd be) and remove.
  This is how to find and fix two sockets made on one face by mistake.
- **The floor bug, a likely fix and a way to see it.** The client rebuilds a
  map block's ground when its neighbours change or arrive, not only the block
  itself (a smoothed top at a block's edge is shaped by its neighbour). And
  while something's grabbed, if the client's copy parts from the server's by
  more than a fifth of a node, the log gets one line twice a second
  (`physics: the client's … nodes off the server's …`): its tier, each
  wheel's hub height, suspension and contact, and the ground blocks under
  it (built, empty or not built) (`World::describe`).

## After that

- **Leaving a world crashed** (every time since sprint 70): the session
  took off `tools.mode`, a verb that was no longer put on, and the throw
  aborted the shutdown, so nothing since was saved. Fixed.
- **What's let go rolls on in the client's physics** (`Body::coasting`):
  corrected toward the server's word as when it was held, drawn where it is
  here, till the server has it still for half a second. Drawn from the
  server's word as it rolled, it jittered.

## Checked

TestPhysics (new: `testClientsGrabbedCartStaysUp`, the client's way of
predicting a grabbed cart on flat ground: it stays up), TestMachine,
TestPiece and TestSculpt; the engine build; a headless boot; and
`bun check:lab`. Not run in the game client.

## The floor bug, found

Wheels sank only while an object was dragged, and only on the dragging
player's screen. The client's copy of a machine's physics was compiled from
the machine its drawing had read, which is re-read only when the object is
next drawn: one edit behind. So the wheel just made wasn't in it, and that
side of the car had nothing under it. It's now compiled from the object's
text as it is (`Body::mirror`). Found by logging what each side compiled:
the same text, 5 parts on the server and 3 on the client.

Also fixed: a crash (a segfault about 45 s in) when what's pointed at was
changed after the object was selected (a machine looked through, sprint
69): the punch used a stale object. A crash means no clean shutdown, so
machine edits weren't saved: that was the "parts gone on reload".

## Left

- Dragging a joint to move it.
- The F float; a joint change mirrored to its twin.
