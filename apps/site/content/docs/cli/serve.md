---
title: terra serve
description: Run the server in this folder with no window, for whoever is allowed to join.
---

# `terra serve`

Run the server you're standing in with no window, for whoever is allowed to join.

```bash
terra serve
terra serve ./my_server
```

It [prepares](/docs/server/lifecycle#prepare), boots, opens its worlds and runs until stopped.
Players join over the internet by the server's address or, for friends, straight from their
friends list. Who may join is in [server.config.ts](/docs/server/setup#who-may-join).

`Ctrl+C` stops it cleanly: everything is saved and everyone connected is told.

```
✓ prepared   3 mods · 2 worlds
✓ booted     212 ms
✓ verdant    open
✓ ember      open
  listening on :30000 · friends may join
```

## Options

| Flag | Default | Description |
|---|---|---|
| `--port <n>` | `30000` | Where to listen |
| `--json` | | Log as JSON lines to stdout, for a supervisor or a log pipeline |

Hosting on a machine you don't sit at, and running a server for good, are covered in
[Deploy](/docs/deploy).
