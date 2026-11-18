---
title: Lifecycle
description: What a Terraverse server does from start to stop. Prepared, booted with its mods, its worlds opened, running with or without players, and reset to where it started.
---

# Lifecycle

What a server does from the moment you start it to the moment it stops, and what keeps
happening in between whether anyone is there or not.

```
prepare  →  boot  →  open worlds  →  run  ⇄  players join and leave  →  stop
                                       ↑                                  │
                                       └────────────  reset  ─────────────┘
```

## Prepare

Before anything runs, the server makes sure it has what it describes. The mods in its
[package.json](/docs/server/structure/package) are installed at exactly the versions `bun.lock`
pins, local mods in `mods/` are built, and [server.config.ts](/docs/server/structure/config) is
checked against all of them: every setting known, every value in range.

Then it checks the worlds against the mods. A world is saved in the names its mods declared:
open it without one of them and its blocks would have nothing to be. So a world last run with
a mod this server no longer has **won't open**, and the server says which world and which mod
before anything is lost.

```
✗ verdant was last run with @you/kilns@0.3.0, which this server doesn't run.
  Install it, or remove verdant from this server.
```

Nothing here runs any mod's code. A server that can't prepare stops before it has touched a
world.

## Boot

Every mod's server half runs, once, in dependency order: a mod you build on has loaded before
yours begins. This is where mods declare what things are (blocks, items, substances) and
register what they react to.

When the last one finishes, **content is sealed**. From here on, the names that exist are the
names a world can be saved in, and they don't change until the next boot.

A mod that throws while booting stops the server, naming the mod and the line. A universe that
opened with half its rules would generate and save land without the other half, and that
damage outlives the bug.

## Open worlds

Each world opens from where it was left. A world with nothing saved yet opens from its
[starting state](/docs/server/setup#the-starting-state) if it has one, or from its world code
alone. Either way the land nobody has touched isn't stored: it is generated again, exactly the
same, from the code.

## Run

From here the server runs continuously: time passes, fire spreads, water finds its level,
crops grow and kilns cool. **It doesn't wait for players.** A server with nobody on it is
still a running universe.

What changes with distance is detail. Near players the world runs **live**, every block and
body simulated. Further out it runs **settled** or **scheduled**: a river keeps flowing as a
rate, a kiln finishes its firing at the time it would have, a cart on a route arrives when it
should. Nothing is lost at the handover: matter, energy and time are conserved, so walking back
to a place finds it as if you had never left.

Everything is saved as it goes. Stopping a server, or losing power, costs seconds, not a
session.

## Players join and leave

```
join   →  checked against who may join  →  sent the universe's rules  →  placed where they left off
leave  →  saved where they stand  →  the world carries on
```

A joining player is sent everything they need to see and touch this universe: what every block
is and how it looks, each mod's assets, and each mod's client half. Assets are cached by
content, so coming back costs only what changed.

They land where they left, on whichever world that was. A first-timer lands where the server's
setup, or its game, says new players start.

## Stop

Stopping saves everything, tells everyone connected, and shuts down. Starting again picks up at
**prepare**: a mod updated in between is fetched, and checked against the worlds, before
anything runs.

## Reset

Reset puts the universe back to its starting state: worlds as they started, players and what
mods kept cleared, the clock back to zero. The rules stay exactly as they were.

For a game that's a new round. For an experiment it's a new trial, from the same start, under
the same rules, so the only thing that differs is what happened.
