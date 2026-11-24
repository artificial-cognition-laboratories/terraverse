---
title: terra init
description: Make a server or a mod, as a folder that's installed, typed and ready to run.
---

# `terra init`

Make a server or a mod, as a folder that's installed, typed and ready to run.

```bash
terra server init my_server
terra mod init kilns
```

The only command that needs the noun: there's no folder yet for it to read.

## server init

```bash
terra server init my_server
```

A [server folder](/docs/server/structure): a `package.json`, a `server.config.ts` with a star
system, one world and you as its admin, and the types in `.terra/`. Run it with
`terra serve`, or play in it with `terra .`.

### From a published server

```bash
terra server init my_arena --from @you/hunger-games
```

A copy of a published server as a project of your own: its config, its local mods and its
starting state, with its mods installed at the versions it pins. Change anything, then host it
or publish it under your own name.

## mod init

```bash
terra mod init kilns
```

A [mod folder](/docs/mods/structure): `server/main.ts`, an empty `client/`, `mod.config.ts`
and `package.json`, installed and typed. `terra dev` runs it straight away.

The folder's name becomes the package name, under your account's scope: `kilns` is
`@you/kilns`, and everything it makes is `kilns:something`.

## Options

| Flag | Description |
|---|---|
| `--from <ref>` | Start from a published server or mod, instead of empty |
| `--json` | Print one line of JSON to stdout and nothing else |

A folder that already exists and isn't empty is refused. `init` never writes over your files.
