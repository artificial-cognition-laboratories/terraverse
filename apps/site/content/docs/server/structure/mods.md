---
title: mods/
description: Mods that belong to this server alone. Ranks, chat rules, local tweaks. Written with the same mod API, listed in package.json as file dependencies.
icon: vscode-icons:folder-type-plugin
---

# mods/

Mods that belong to this server alone: ranks, a welcome message, a tweak to how an installed game
plays here. Each is a full [mod](/docs/mods/structure), written with the same API, and listed in
`package.json` as a `file:` dependency.

```bash
mods/
└── ranks/
```

They travel with the server when you publish it, never on their own.

Not designed yet.
