---
title: Updates
description: Changing a deployed Terraverse server's rules without taking it down. Checked against the live save first, swapped in without dropping players, and rolled back in one command.
---

# Updates

Change the rules locally, then deploy them.

> Managed hosting isn't available yet. This is how it will work.

```bash
terra install @you/kilns@0.4.0
terra deploy
```

A deploy after the first sends **source only**: `package.json`, `server.config.ts`, `mods/` and
`start/`. The save stays where it's being played.

## Checked against the real save

Before anything changes, the new rules are [prepared](/docs/server/lifecycle#prepare) against
the live save, not your local copy. A mod removed while a world online still has its blocks, a
setting out of range, a major upgrade you haven't confirmed: each stops the deploy, with the
reason, while the running server carries on untouched.

```
✗ verdant (online) was saved with 41 kilns: blocks, and this deploy removes @you/kilns.
  Nothing changed. The running server is still 1.3.0.
```

A bad deploy never takes a good server down.

## No downtime

The new version boots beside the old. When it's ready, players are moved across, the save
handed over between ticks with nothing lost. If it fails to boot, it's thrown away and the old
one never noticed.

Changing the starting state doesn't touch the save: `start/` is used from the next reset.

## Rolling back

```bash
terra deploy --rollback
```

Puts the previous version's rules back the same way. Every deploy is kept, so you can go back
further, to any of them.

A rollback changes rules, not history: what players did under the new version stays done. To
undo *that*, rewind the save. See [Saves](/docs/deploy/saves).

## Engine updates

When Terraverse itself updates, your server moves to the new version on its next deploy, or at a
quiet hour you choose. Mods keep the API version they were built against, so nothing you run
breaks because the engine moved.
