---
title: terra publish
description: Publish the mod or server in this folder to the registry, under your account's scope.
---

# `terra publish`

Publish the mod or server you're standing in to the registry, under your account's scope.

```bash
terra publish
```

It publishes the version in `package.json`. A version is published once: change anything and
bump it.

## A mod

What its layout says belongs: `package.json`, `mod.config.ts`, `tsconfig.json`, the README and
licence, and `server/`, `client/`, `shared/` and `assets/`. Never `.terra/`, `node_modules/`,
tests or hidden files. Mods it depends on travel as names, installed by whoever installs yours.

Its name, version, description and dependencies come from `package.json` and nowhere else:
a `mod.config.ts` that sets any of them is refused. Every dependency is pinned to an exact
version.

The README goes up with it, and so do the images and videos it links to by relative path
(`![a kiln](docs/kiln.png)`). They're stored beside the version, never inside the mod, so
installing it never downloads them.

## A server

Not built yet. A server will publish its rules and its starting state, once worlds can
travel as their changes to the seed.

## Before it goes

Everything `prepare` checks, plus: you're [logged in](/docs/cli#your-account), the scope is
yours, the version is new, and the build passes. A refusal says which.

## Options

| Flag | Description |
|---|---|
| `--dry-run` | List every file and README image that would go, and publish nothing |
| `--json` | Print one line of JSON to stdout and nothing else |
