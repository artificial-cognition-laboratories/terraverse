---
title: terra
description: The Terraverse command line. Open the game, make and run servers, and make, install and publish mods. It reads the folder you stand in. Also installed as terraverse.
---

# `terra`

The Terraverse command line. It opens the game, makes and runs servers, and makes, installs
and publishes mods. It reads the folder you're standing in, so most commands take no
arguments.

```bash
terra                   # open the game
terra --help            # every command, one line each
terra --version
```

It's also installed as `terraverse`, which does exactly the same thing.

## Playing

```bash
terra @you/hunger-games     # a published server, ready to play
terra TF9.dGVycmE           # a fresh universe from a world code
terra ./my_server           # a server folder on this machine
```

**[A bare reference](/docs/cli/ref)** opens something straight away: no menus, straight into
the world.

## Making a mod

```bash
terra mod init kilns
cd kilns
terra dev               # a scratch universe with your mod in it, reloading on save
terra publish
```

**[`init`](/docs/cli/init)** makes a folder that's installed and typed, ready to edit.
**[`dev`](/docs/cli/dev)** opens the game in a universe running your mod, and reloads it every
time you save. **[`publish`](/docs/cli/publish)** puts it on the registry.

## Running a server

```bash
terra server init my_server
cd my_server
terra install @terraverse/survival @you/kilns
terra serve             # headless, for whoever's allowed to join
```

**[`install`](/docs/cli/install)** adds a mod to the server's rules, checked against its
worlds. **[`serve`](/docs/cli/serve)** runs it with no window.

## One grammar, two kinds

A server and a mod are both projects, and they take the same verbs:

| | make | run | regenerate types | share |
|---|---|---|---|---|
| server | `terra server init` | `terra serve` · `terra dev` | `terra prepare` | `terra publish` |
| mod | `terra mod init` | `terra dev` | `terra prepare` | `terra publish` |

Only `init` needs the noun, because there's no folder yet to read. Every other verb works out
what kind of project it's standing in from its `server.config.ts` or `mod.config.ts`, and does
the right thing.

## Your account

```bash
terra login             # your ArcLabs account: needed to publish, and to host online
terra whoami
terra logout
```

terra signs in with its own key, separate from the game's: the game's can only play, and
terra's can publish. `ARCLABS_URL` points terra at another server (local staging is
`http://localhost:3099`), with its own sign-in. Where nobody can approve a sign-in (CI
publishing a mod), `TERRA_TOKEN` gives terra a key directly: it wins over the saved sign-in,
and `terra login` and `terra logout` refuse while it's set.

```bash
terra info @you/kilns                        # its versions, and the mod API each needs
terra deprecate @you/kilns "use @you/furnaces"
terra deprecate @you/kilns --undo
```

## Scripting it

`--json` makes any command print one line of JSON to stdout and nothing else. Errors go to
stderr with a non-zero exit, and say what was expected, so a script can tell a refusal from a
crash.

```bash
terra install @you/kilns --json
```
