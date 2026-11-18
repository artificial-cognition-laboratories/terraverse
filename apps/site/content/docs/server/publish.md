---
title: Publish
description: Sharing a Terraverse server on the registry. Its rules and its starting state, as a game anyone can open and play from the start.
---

# Publish

A published server is a game anyone can open: its rules and its starting state, on the
registry under your account.

```bash
terra login
terra publish
```

Players open it with `terra @you/hunger-games`, or from the game's browser, and start playing
from day one. Builders start from it with `terra server init --from @you/hunger-games`.

## What goes

The source half, and nothing else:

| Goes | Stays |
|---|---|
| `package.json` · `server.config.ts` | `save/`: your players and history |
| `mods/`: your local mods, built | `.terra/`: generated |
| `start/`: the starting state | |

Its mods travel as names and versions, installed when someone opens it. Nobody downloads
your universe, only the rules and the place it starts.

Servers are `"private": true` until you remove it from `package.json`, so one is never
published by accident.

## Whoever opens it is in charge

A published server's `access` is a template. Whoever opens it is its admin; your own account
in `roles` isn't carried over. The roles you defined, and what each may do, are.

```ts
access: {
    join: "friends",                 // the opener's friends, not yours
    roles: { admin: ["@cody"] },     // becomes whoever opened it
}
```

## Opening one makes a universe

Opening a published server installs a universe of its own: its mods fetched at the pinned
versions, its starting state copied, a fresh save. Everyone who opens it gets their own, and
what happens in yours never reaches anyone else's.

## Updates

Publish a new version and everyone who opened an older one is offered it:

- **Rules** (mods, settings, local mods) update as a [mod upgrade](/docs/server/mods#changing-the-rules) would: checked against their worlds first, and a new major version asks.
- **The starting state** is used from their next reset. It never touches a save in progress:
  you rebuilt the arena, but their match keeps the arena it started in.

Versions are promises, as for [mods](/docs/mods/publish#versions-are-promises). A version is
published once and never changes, and nothing is ever deleted, only deprecated.

## Before it goes

Publishing refuses, with the reason, if you're not logged in, the scope isn't yours, the version
already exists, a mod it runs isn't published, or `server.config.ts` doesn't check against its
mods. A local mod in `mods/` is fine: it travels with the server.
