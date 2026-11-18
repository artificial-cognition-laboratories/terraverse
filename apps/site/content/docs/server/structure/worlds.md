---
title: worlds/
description: The planets this server runs, one folder each. Written by the server as they play, and opaque on purpose. Copy one to back it up or move it.
icon: vscode-icons:folder-type-db
---

# worlds/

The planets this server runs, one folder each, in `save/`, named as in
[server.config.ts](/docs/server/structure/config).

```bash
worlds/
├── terra/
└── ember/
```

What's inside is the server's business: the land that changed, the simulation in flight,
what mods saved for that planet. None of it is yours to edit, and nothing in it is setup.
A world's code and name come from `server.config.ts`; this is only the record of what
happened there.

What you can do with one:

| | |
|---|---|
| **Back it up** | Copy the folder |
| **Move it** | Copy it into another server's `worlds/`, with the same mods |
| **Reset it** | Delete it. Untouched land comes back exactly, from its world code |

Not designed yet.
