---
title: .terra/
description: Everything the server generates for itself. Types, logs and built local mods. Regenerable: delete it freely.
icon: vscode-icons:folder-type-typings
---

# .terra/

Everything the server makes for itself. Never edit it, never commit it, delete it freely.

```bash
.terra/
├── types/      # the type frame server.config.ts and mods/ are checked against
├── build/      # your local mods, built
└── logs/       # what happened, and what failed, by day
```

Delete it and the next run rebuilds everything but the logs. If something is acting strangely,
this is always safe to clear. `save/` never is.

Not designed yet.
