# Sprint 56 — The workbench

- **F puts it on the workbench**: it lifts half a node where it is and
  floats there, anchored and level; the player walks round it with their
  own camera and the tools work on it. Its disc and ring turn it: pressed
  with the crosshair on them, it turns as you look round, in steps. F
  again puts it down. The held-in-front view, the orbit camera and the
  wheel's zoom are gone (`Hold` is the bench: `watch` the player).
- **Tab frees the cursor** (the rails, the menus; the world's pointed at
  through it: a ray from the screen); Tab again looks. A part's menu
  frees it too. Face-me (Tab's last job) is gone.
- **New part drawn on the world is a new object** of what's drawn, where
  it's drawn (a plate: a box dragged on the floor).
- **Sockets** (`Machine::Socket`, TFM3): a joint waiting for a part, made
  from a face's menu (right-click it on the bench: Add hinge / Add weld,
  mirrored), drawn green (a stub and a ring). Carried near one, an object
  snaps to it first (`Machine::plug`: its face against the socket's,
  middles met, joined as the socket says) and the socket's taken up.
  The card lists a part's sockets, each removable.

Proved: TestMachine `testSockets` (written and read back; plugged flush,
hinged about its axis, taken up; taken off with its part: fails with the
axis turned), TestSculpt hold tests rewritten for the bench (anchored
pose, the across axis as the player walks round).

Not seen on screen. Turning by look with the crosshair on the handle may
want a sense of how far; the menu's at the screen's middle when it's opened
with the crosshair.
