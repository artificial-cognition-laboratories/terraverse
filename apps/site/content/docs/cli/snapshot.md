---
title: terra snapshot
description: Record the server's worlds as its starting state, or reset the server back to it.
---

# `terra snapshot`

Record the worlds of the server you're standing in as its
[starting state](/docs/server/setup#the-starting-state).

```bash
terra snapshot
```

Build the arena, stock the chests, place the spawn, then snapshot. The worlds as they are now
are written to `start/`, the state every reset returns to and the state a published server
opens in. Players and what mods kept aren't part of it: a starting state is the place, not the
people.

The server must be stopped, so what's recorded is a moment, not something half-saved.

## terra reset

```bash
terra reset
```

Return the server to its starting state: worlds back to `start/`, or to their world code if
there isn't one, with players and what mods kept cleared. Its rules don't change.

It asks first, naming what will be lost. A save can't be made again.

## Options

| Flag | Description |
|---|---|
| `--yes` | Reset without asking |
| `--world <name>` | Snapshot or reset only this world |
