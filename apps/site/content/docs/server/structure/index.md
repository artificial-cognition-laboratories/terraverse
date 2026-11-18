---
title: Server structure
description: A Terraverse server is a folder, and the folder is a TypeScript project. Source at the top, the save in one folder, and everything regenerable in .terra/.
---

# my_server

A server is a folder, and the folder is a TypeScript project: keep it in git, publish it,
deploy it.

```bash
my_server/
├── .terra/           # generated: types, logs, builds. regenerable
├── mods/             # your own mods, for this server only
├── save/             # the universe as it's been played. never edit, always back up
│   ├── worlds/
│   ├── players/
│   └── store/
├── start/            # the worlds as they start, which reset returns to
├── package.json      # its name, its version, and the mods it runs
├── server.config.ts  # the star system, worlds, settings and who may join
└── tsconfig.json
```

`terra server init my_server` creates it; `terra host` runs it.

## Three kinds of thing

| | | Edit | Commit | Delete |
|---|---|---|---|---|
| **Source** | `package.json` · `server.config.ts` · `mods/` · `start/` | Yes | Yes | No |
| **Save** | `save/` | Never | No | **Never** |
| **Generated** | `.terra/` · `node_modules/` | Never | No | Freely |

**Source** is the rules: clone it somewhere new and you have the same universe, from the
start. **The save** is what happened in it, and nothing can make it again. **Generated** is
rebuilt on the next run; if something's acting strangely, deleting it is always safe.

That's why the save isn't hidden in a dotfolder. A dotfolder reads as a cache, and caches get
deleted.

## A server is a project

It has a `package.json`, a `tsconfig.json`, a `bun install` and a generated type frame, the
same as a mod. Which is why `server.config.ts` knows every setting of every mod you've
installed, with no imports:

```ts
settings: {
    "@you/kilns": { kilnHeat: 1500 },   // typed from @you/kilns's mod.config.ts
}
```

## Universe-wide, or one world's

A player walks off one planet and lands on another as the same person, carrying the same
things. So whatever a player is, and whatever a mod keeps for the whole universe, lives in
`save/` beside the worlds rather than inside one. A world holds only what belongs to its
planet: the land, and what is happening on it.
