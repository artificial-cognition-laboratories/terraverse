# Sprint 48 — Blueprints

## Decision

You always edit a blueprint; the world holds instances of one. A design is
free: a ghost, drawn see-through, passed through. Made real it's solid (in
creative, free; survival's cost is a process per material, later).

1. **A piece knows who it is.** An id of its own, kept on disk and the
   wire (TFP2), and ghost or real. Undo names pieces by it, not by the
   entity's object id (reused, lost on restart).
2. **Workshop** (`src/workshop/`): the server's side of the tools: strokes,
   undo, new pieces, the library. Packet handlers parse and delegate;
   refusals are words told the player, and logged.
3. **Library**: each player's blueprints by name in the world
   (`<world>/blueprints/<player>/<name>.tfb`, TFB1: parts, each a piece and
   its offset: room for a machine's pieces, joints after).
4. **The rails**: shaping tools left, everything else right (focus,
   mirror, undo, new piece, blueprints, save, make real), one button
   style: the pause menu's profile button (`kit/IconButton.vue`).

## Done (2026-10-29)

- Protocol 67: TOSERVER_BLUEPRINT_SAVE / _PLACE, TOSERVER_PIECE_REAL,
  TOCLIENT_BLUEPRINTS (on join and on save).
- `piece::header` (was `extent`): id, ghost, box; a ghost's entity isn't
  physical nor collided with (ObjectProperties::validate).
- New pieces start as ghosts; a TFP1 piece reads as real, given an id.
- Keys and the right rail go through one path: `Hand::ask` → `Game::handleAsk`
  (Hand::ACTIONS is the table both read).
- Page: `tools.piece` (ghost/real/null), `library` section, `tools.ask`.

Proved: TestPiece `testKeepsWhoItIs` (ids, ghost, renew, TFP1), ghost tie in
`testSentWithItsEntity`; TestLibrary (format, per player, names refused:
paths, dots, spaces at the ends). Each fails with its piece sabotaged.

Not proved: the ghost drawn see-through (vertex alpha + blend, unseen),
the rails on screen, save/place/real end to end (no Workshop test: it
needs a ServerEnvironment), typing a name while the game reads keys.
