# Game

Terra Firma: the engine and the game. A fork of Luanti (C++, LGPL-2.1+) that keeps its
rendering, meshing, networking and map storage, and replaces its shell, world generation,
movement and much of its simulation. What the game is and the rules for changing it are in
the repo's `AGENTS.md`; players start at [terraverse.arclabs.it](https://www.terraverse.arclabs.it/).

```
src/client/      the client: app (game loop, menus, input), render, views, world
  app/shell/     the web UI host: embedded Chromium, the bridge, and interface/ (the Vue app)
src/server/      the server: app (access, mods sidecar, network), game, players, world
src/shared/      what both sides use: base, game, map, platform, util
library/         the pure libraries: world (terragen), matter, physics, space, life, geometry
server/          the official TypeScript mods' server folder, run by every world not given another
assets/          shipped data: matter, world, textures
tests/           unit modules, library tests, probes, the runners (run/) and whole-engine checks (checks/)
scripts/         the dev loop, build, staging, bakes (bake/), releases (release/)
```

## Building and running

```bash
bun run build                 # engine and production UI, into .output/
bun dev                       # engine, Vite and the game, hot-reloading the UI (from the root)
bun run stage                 # refresh resources in the runtime tree
../../.output/bin/luanti --server --gameid terrafirma --world <tmp dir> --port <n>   # headless boot
```

## Checking

```bash
bun test:hot                  # the pure libraries' quick tests: the loop to keep running
bun run test                  # every library test and engine unit module
bun run check                 # before a commit
bun check:land                # after a world generation change
bun check:wiki                # every wiki scene, through the real code
bun check:mods                # TypeScript mods in a headless server
```

`package.json` has the rest (`check:matter`, `check:moves`, `check:far`, `check:registry`, …).
Releases go through `arc deploy terraverse`.

## Docs

- [Installing the game](https://www.terraverse.arclabs.it/docs/install)
- [Writing mods](https://www.terraverse.arclabs.it/docs/mods), the TypeScript API the engine runs
- [Running a server](https://www.terraverse.arclabs.it/docs/server)
