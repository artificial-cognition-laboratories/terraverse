---
title: Installation
description: Install Terraverse on Linux with one command, or from a .deb or AppImage.
---

# Installation

On Linux, one command installs the launcher:

```bash
curl -fsSL https://terraverse.arclabs.it/install.sh | sh
```

It also puts [`terra`](/docs/cli) on your `PATH` (as `terraverse` too), when `~/.local/bin` is
on it. On its own it opens the launcher; with a command, `terra dev`, `terra host`, it runs the
command line of the version you last played, so it always matches the game it runs.

The launcher is also published as a `.deb` and an `.AppImage`. A Windows build is on its way.
