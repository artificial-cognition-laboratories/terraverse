---
title: terra install
description: Add mods to the server in this folder, pinned to exact versions and checked against its worlds.
---

# `terra install`

Add mods to the server you're standing in, pinned to exact versions.

```bash
terra install @terraverse/survival
terra install @you/kilns@0.3.0 @sam/bellows
terra i @you/kilns
```

Each mod, and what it depends on, is added to the server's
[package.json](/docs/server/structure/package) as a dependency. It's `bun add` with the checks
a universe needs:

- **exact versions**, never ranges: the latest, unless you name one
- **prefixes that don't collide**: two mods that would both make `kilns:` things are refused,
  naming both
- **worlds that still open**: changing a mod's version is checked against what each world was
  saved with

```
✓ @you/kilns@0.3.0      kilns:
  + @terraverse/craft@1.4.2  (needed by kilns)
  settings: kilnHeat (number, default 1300 K)
```

The mod's settings are listed, so you know what [server.config.ts](/docs/server/setup#settings)
can now choose.

## Upgrading

```bash
terra install @you/kilns@0.4.0
```

Installing a mod you already have changes its version. A new major version is the author saying
it may not fit your worlds, so it asks before going ahead.

## Options

| Flag | Description |
|---|---|
| `--yes` | Accept a major-version upgrade without asking |
| `--json` | Print one line of JSON to stdout and nothing else |
