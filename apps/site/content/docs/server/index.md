---
title: Server
description: A Terraverse server is one universe under one set of rules, running its worlds for whoever joins. Singleplayer is a server too.
---

# Server

A server runs one game: a universe of worlds, under one set of rules. It runs a star system's
worlds for whoever joins, with the same mods in every one of them.

```
server            the universe: its rules, its players, who may join
├── mods          the rules: what things are and how they behave
├── players       the people in it, the same person on every planet
└── worlds        the planets of its star system, one world each
```

Everything else is built from that. A survival server for friends, a Hunger Games arena, a
race track and a lab running a hundred experiments are all servers, set up differently.

## There is always one

Playing alone, you're still on a server: the game makes one on your machine, runs your worlds
in it, and connects you as its only player. Opening it to friends lets them join the same
server. A dedicated server is the same thing with no game window, kept in a folder you can put
in git.

So there's one model to learn, and a mod written for one of these runs on all three.

## One set of rules

A server's mods are the universe's rules. Every world on it runs the same set, so walking off
one planet and onto another never changes what water is or how a kiln works. A game is just a
mod that sets more of them: where players start, what they carry, what wins.

Each mod's settings are chosen once, for the whole server. How hot a kiln burns is a fact
about this universe, not about one planet in it.

## Worlds are planets

A server has one star system, from a [world code](/docs/server/setup#the-star-system), and
each world is one of its planets. They share a sun and real orbits, so getting from one to
another is a journey, not a teleport.

A world holds only what belongs to its planet: the land that has changed, and what is
happening on it. Players and what mods keep for the whole universe live on the server, which
is why you're the same person, carrying the same things, wherever you land.

## Where it starts

A server has a starting state: its worlds as they should be on day one. A survival server
starts from bare planets. A Hunger Games server starts from its arena, built and stocked.
**Reset** returns it there: a new round for a game, a new trial for an experiment.

## Shared like a mod

Publish a server and you publish its rules and its starting state, never its players or its
history. Its mods travel as names, installed on the way in. What players call *a game* is a
published server: open it, and you're playing it from the start.

## Where next

- **[Lifecycle](/docs/server/lifecycle)**: what a server does from boot to reset.
- **[Setup](/docs/server/setup)**: making one, from the star system to who may join.
- **[Mods](/docs/server/mods)**: choosing its rules.
- **[Publish](/docs/server/publish)**: sharing it, as a game anyone can open.
- **[Server structure](/docs/server/structure)**: the folder, file by file.
