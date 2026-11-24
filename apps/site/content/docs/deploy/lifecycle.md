---
title: Lifecycle
description: A deployed Terraverse server from its first upload to running, sleeping, waking and being taken down, and how that differs from running one yourself.
---

# Lifecycle

A deployed server lives the same [lifecycle](/docs/server/lifecycle) as one on your machine:
prepare, boot, open its worlds, run. What's different is everything around it.

> Managed hosting isn't available yet. This is how it will work.

```
first deploy  →  running  ⇄  asleep
                    ↑
              terra deploy (updates)
```

## The first deploy

```bash
cd my_server
terra deploy
```

The first time, the whole universe goes up: its source and its `save/`, so the worlds you've
been playing in carry on exactly where they were. It gets an address under your account, and
starts.

From then on **the save lives in the cloud**. It's the one that's played in, and your local
`save/` is only a copy from the moment you deployed. Later deploys send source, never the save,
so nothing you do locally can overwrite what people have built online.

## Running

Exactly what `terra serve` does: the same engine, the same version, the same mods at the same
pins. Players join by its address, or from the public list if you've chosen to be on it. Who may
join is still `server.config.ts`.

Logs, players online and each world's state are on the dashboard, and commands run from its
console as they would from `terra serve`.

## Asleep

When the last player leaves, the server sleeps: the universe runs scheduled, with time passing
but nothing simulated live. It costs almost nothing.

A player joining wakes it in seconds. Because it never stopped keeping time, there's nothing to
catch up on: the world they land in is where it should be.

## Taken down

```bash
terra deploy --down
```

Stops it and keeps everything: the save, its history and its address, for when you bring it
back. Deleting a deployment is separate, asks twice, and offers the save as a download first.

## What's different from your machine

- **The save is the cloud's.** Deploys send source; the save only moves when you ask it to.
- **It never really stops.** Sleep instead of off, so time keeps passing.
- **Restarts don't drop players.** Updates start the new version beside the old and move
  everyone across. See [Updates](/docs/deploy/updates).
- **Backups aren't yours to remember.** See [Saves](/docs/deploy/saves).
