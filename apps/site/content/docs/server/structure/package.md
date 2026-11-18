---
title: package.json
description: A server's name, its version, and the mods it runs, as dependencies pinned to exact versions.
icon: vscode-icons:file-type-json
---

# package.json

Your server's name, its version, and the mods it runs.

```json
{
    "name": "@you/hunger-games",
    "version": "1.0.0",
    "description": "Twenty-four players, one arena, one winner.",
    "private": true,
    "type": "module",
    "dependencies": {
        "@terraverse/survival": "1.2.0",
        "@you/kilns": "0.3.0",
        "ranks": "file:./mods/ranks"
    }
}
```

## The mods are the dependencies

Every mod in `dependencies` runs, and nothing else does. There's no second list to keep in
step: install a mod and it's a rule of this universe; uninstall it and it isn't.

```bash
terra install @you/kilns       # bun add, pinned, then checked against your worlds
terra uninstall @you/kilns     # refuses if a world was saved with its blocks
```

`terra install` is `bun add` with the checks a universe needs: exact versions only, prefixes
that don't collide, and worlds that will still open. Edit the file by hand if you like; the
server checks the same things when it starts.

Local mods in `mods/` are `file:` dependencies, so they load, resolve and type exactly like
published ones.

## Pinned, always

Versions are exact, never ranges, and `bun.lock` beside this file pins what they depend on.
The universe you run is the universe anyone who clones it runs.

## name and version

What the server is called on the registry, and which release of it this is. Remove `private`
when you mean to [publish](/docs/cli/publish) it. A published server is installed by name, and
players who open it get this exact version, rules and starting state together.
