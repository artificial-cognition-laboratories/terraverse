---
title: Publish
description: Sharing a Terraverse mod on the registry. Versioned under your account, installed by name, and kept forever, because worlds depend on it.
---

# Publish

A published mod is on the registry under your account, versioned, and installable into any
server by name.

```bash
terra login
terra publish
```

That publishes the version in your `package.json`. From then on anyone can run
`terra install @you/kilns`, and your mod is one of their universe's rules.

## What goes

Only what the mod uses: its server and client halves, bundled from what they import, its
`assets/`, and its `mod.config.ts`. Mods it builds on travel as names and versions, installed
alongside it by whoever installs yours.

Never your `.terra/`, your tests or anything the mod doesn't reach. `terra publish --dry-run`
lists exactly what would go.

## What the registry shows

Read from `mod.config.ts` and `package.json`, without running anything: the title and
description, every setting a server can choose with its range and default, the prefix your
things are named by, what it builds on, and the API version it was built against.

So a server's admin can see what a mod will ask of them before they install it.

## Versions are promises

A version is published once and never changes. Worlds are saved in your mod's names, and a
server pinned to `0.3.0` must get exactly `0.3.0` every time it starts, on any machine, for as
long as it runs.

So versions mean something:

| Bump | When | A server upgrading |
|---|---|---|
| Patch `0.3.1` | Fixes. Nothing anyone relies on changes | Just does |
| Minor `0.4.0` | New things: blocks, settings, messages | Just does |
| Major `1.0.0` | Something changes or goes: a block removed, a setting renamed | Is asked first |

Removing a block is the big one. A world saved with it can't open without it, so a major
version that drops a block should say what replaces it.

## Nothing is ever deleted

You can **deprecate** a version, or the whole mod, with a reason: it's hidden from search, and
servers running it are told on their next start. You can't delete one. Somewhere a world is
saved in its names, and deleting it would leave that world unable to open.

## The API you built against

What you publish records the version of the mod API you built against: the one your terra wrote into `.terra/types/`.
The engine keeps running mods written for older API versions, so a mod published today still
loads years from now, unchanged. Moving to a newer API version is something you choose, not
something done to you.

## Before it goes

Publishing refuses, with the reason, if you're not logged in, the scope isn't yours, the version
already exists, the types don't check, or a block names an asset that isn't there.
