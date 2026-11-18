---
title: terra uninstall
description: Remove mods from the server in this folder, refusing while a world still needs them.
---

# `terra uninstall`

Remove mods from the server you're standing in.

```bash
terra uninstall @you/kilns
```

A world saved with a mod's blocks can't open without it, so before removing anything, this
checks every world:

```
✗ verdant was saved with 41 kilns: blocks.
  Remove them in game, or reset verdant, then uninstall.
```

Nothing is removed while any world still needs the mod. Mods that were only installed because it
needed them are removed with it.

## Options

| Flag | Description |
|---|---|
| `--json` | Print one line of JSON to stdout and nothing else |
