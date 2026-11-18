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

Bundled with only what it imports: its server and client halves, its assets, and its
`mod.config.ts`. Mods it depends on travel as names, installed by whoever installs yours.

## A server

Its rules and its starting state: `package.json`, `server.config.ts`, `mods/` and `start/`.
Never `save/`. Its mods travel as names, installed when someone opens it. Remove
`"private": true` from `package.json` first, so a server is never published by accident.

## Before it goes

Everything `prepare` checks, plus: you're [logged in](/docs/cli#your-account), the scope is
yours, the version is new, and the build passes. A refusal says which.

## Options

| Flag | Description |
|---|---|
| `--dry-run` | Show what would be published, and publish nothing |
| `--json` | Print one line of JSON to stdout and nothing else |
