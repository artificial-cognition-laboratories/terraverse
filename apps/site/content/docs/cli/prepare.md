---
title: terra prepare
description: Bring the project in this folder up to date. Install what package.json pins and regenerate its types.
---

# `terra prepare`

Bring the project you're standing in up to date: install what `package.json` pins, and
regenerate the types in `.terra/`.

```bash
terra prepare
```

`dev`, `host` and `install` all prepare first, so you rarely need it by hand. Run it when your
editor has fallen behind: after pulling someone's changes, or switching branches.

In a server it also checks `server.config.ts` against the mods it runs: every setting known,
every value in range, every world openable. Nothing is run, and a problem is reported with the
file and line.
