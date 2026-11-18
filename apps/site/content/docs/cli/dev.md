---
title: terra dev
description: Run the mod or server in this folder in the game, reloading every time you save.
---

# `terra dev`

Run the project you're standing in, in the game, and reload it every time you save.

```bash
terra dev
```

## In a mod

Opens the game in a scratch universe running your mod and what it depends on, with you as its
admin in creative. The scratch universe is kept in terra's cache, one for each mod, so its
world is still there next time. `--fresh` starts it from nothing instead.

Save a file and the mod reloads: it starts again from nothing, so every handler, timer and
panel from the last run is gone and nothing piles up. The world and everyone in it carry on.
Client halves reload on your screen too.

Change what your mod *declares*, a block added or a definition changed, and the universe
restarts with it, a couple of seconds, and the game reconnects. A world can't swap the blocks
it's made of while it runs, so this is rather than half-applying the change. A save that
doesn't build is shown with its file and line, and the last build keeps running.

```bash
terra dev --in ../my_server
```

`--in` runs your mod inside a real server instead, with its rules, its settings and a copy of
its save: how your kiln mod plays alongside a survival game, without touching the real one.

## In a server

Runs the server and opens the game in it, reloading its local mods in `mods/` as you save.
Installed mods don't reload: they're pinned, and they change only when you install another
version.

## Options

| Flag | Description |
|---|---|
| `--in <server>` | In a mod: run inside this server folder, on a copy of its save |
| `--fresh` | Start from an empty save every run |
| `--headless` | Run without opening the game |
