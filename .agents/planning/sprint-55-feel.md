# Sprint 55 — Feel

- **Drawn where this client says** (`GenericCAO::setPosed`): a carried or
  held object is put where it's carried every frame; the server's told 20
  times a second and has the last word when it's let go. No stutter.
- **Carried ahead**: its offset kept in the view's right/up/forward, and
  turned as the player turns: it stays where you look.
- **Lit when pointed at** in the world (`Look::lit`), with what a press does.
- **The tools say what the left button does** (Cody's A): tools away, it
  carries; out, it draws on what's pointed at, terrain or object, where it
  is. F picks one up to work on.
- **Shapes in two steps**: drag the footprint, let go, the depth follows
  the cursor out of (or into) the face (`sculpt::nearestAlong`; a stroke's
  `deep`), click to make it; Esc or the other button stops it.
- **Pull**: press on a face, its flat stretch (spread across its plane,
  client side) is pulled as it's held: out adds, in cuts; let go to make it.
- **Rails**: Select and the brushes on the left, the shape behind one
  button; the right is the rest. A right-click on a held object's part
  opens its menu there (its card: joints, detach, delete).

Proved: TestSculpt (a pulled stroke's cells and measure, its depth's
bounds; `nearestAlong` up and below, and along: fails with its sign
turned). The rest is on screen only, not seen: the carried object's turn
with the player (yaw's sign), the menu's position under the page's scale.
