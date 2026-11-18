---
title: .terra/
description: Everything the server generates for itself. Types, logs and built local mods. Regenerable: delete it freely.
icon: vscode-icons:folder-type-typings
---

# .terra/

Everything the server makes for itself. Never edit it, never commit it, delete it freely.

```bash
.terra/
├── manifest.json   # what runs: each mod, in load order, with the settings it was given
├── build/          # each mod's server half, built, with a copy of its assets/
├── types/          # the type frame server.config.ts and mods/ are checked against
└── logs/           # what happened, and what failed, by day
```

`manifest.json` and `build/` are all a server needs to run, and they point at nothing outside
`.terra/`, so a prepared server can be moved or shipped as it is. The game's own mods run that
way: its server folder is prepared when the game is built, and only those two travel with it.

Delete it and the next run rebuilds everything but the logs. If something is acting strangely,
this is always safe to clear. `save/` never is.

Not designed yet.
