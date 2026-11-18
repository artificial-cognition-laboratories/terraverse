---
title: Saves
description: A deployed Terraverse server's save. Backed up continuously, rewindable to any point, forkable, and always downloadable as an ordinary server folder.
---

# Saves

The save is the one part of a universe nothing can make again. A deployed server looks after it.

> Managed hosting isn't available yet. This is how it will work.

## Backed up as it plays

There's nothing to schedule. The save is kept continuously, with history: every change for the
last day, hourly for the last month, daily beyond that.

## Rewind

```bash
terra deploy --rewind "2 hours ago"
terra deploy --rewind 2026-11-18T14:00
```

Puts the universe back to how it was, all of it or one world, for when someone burns down the
town. Rules don't change; only what happened does. The state you rewound from is kept too, so a
rewind can be undone.

## Fork

```bash
terra deploy --fork verdant-test
```

A second deployment, from a copy of this one's save at this moment: try the new mod on the real
universe, run an event, or keep a universe as it was before a reset. What happens in a fork never
reaches the original.

## Take it home

```bash
terra deploy --pull
```

Downloads the save into your folder's `save/`, so the whole universe runs on your machine with
`terra host` exactly as it did online. Nothing is in a format only we can read: leaving costs one
command.

## Snapshots and resets

`terra snapshot` and `terra reset` work on a deployment as on your machine: record the starting
state from the live worlds, or return to it. A reset is kept in history like any other change,
so it can be rewound.
