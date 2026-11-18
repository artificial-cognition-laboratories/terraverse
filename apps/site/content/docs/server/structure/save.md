---
title: save/
description: The universe as it has been played. Its worlds, its players and what mods kept. Written by the server, never edited, always backed up.
icon: vscode-icons:folder-type-db
---

# save/

The universe as it has been played: every world, every player, and everything mods kept.

```bash
save/
├── worlds/     # one per planet: the land that changed, and what's happening on it
├── players/    # everyone who has joined: body, inventory, which world they're on
└── store/      # what mods kept for the whole universe
```

**Nothing here can be made again.** Generated land comes back from the world code; what people
built on it doesn't. Back this folder up, and never delete it to fix something: that's what
`.terra/` is for.

What's inside is the server's business. None of it is yours to edit, and nothing in it is
setup: a world's code and name come from `server.config.ts`, and this is only the record of
what happened.

| | |
|---|---|
| **Back it up** | Copy the folder |
| **Move the universe** | Copy it into another server with the same mods |
| **Start over** | Reset, which returns to `start/`, or delete it to start from the world code |

Not designed yet.
