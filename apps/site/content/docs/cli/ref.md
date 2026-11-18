---
title: terra <ref>
description: Open something straight away. A published server, a world code, or a server folder, straight into the world.
---

# `terra <ref>`

Open something straight away: straight into the world, no menus.

```bash
terra @you/hunger-games         # a published server
terra @you/hunger-games@1.2.0   # a published server, at one version
terra TF9.dGVycmE               # a fresh universe from a world code
terra ./my_server               # a server folder
terra .                         # the server folder you're in
```

What a reference is decides what happens:

| Reference | Opens |
|---|---|
| `@scope/name` | A published server. The first time, it's installed as a universe of your own, its mods fetched and its starting state copied; after that, the same universe, where you left it |
| A world code | A new universe on that star system, with no mods: the clean universe, nothing on top |
| A path | That server folder, as it is |

A published server you open this way is yours to play, kept with the game's other universes.
To make one into a project you can change, use
[`terra server init --from`](/docs/cli/init#from-a-published-server).

## Options

| Flag | Description |
|---|---|
| `--world <name>` | Land on this world, instead of where you left off |
| `--host` | Open it to whoever's allowed to join while you play |
