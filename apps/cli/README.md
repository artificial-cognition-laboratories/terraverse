# terra

The Terraverse CLI: make servers and mods, run them in the game as you edit, and publish
mods to the registry. It also runs TypeScript mods for the engine, as its sidecar
(`terra runtime`).

```bash
terra mod init kilns --scope @you   # a new mod, installed and typed
cd kilns && terra dev               # play it, reloading on every save
terra publish                       # onto the registry, under your @scope
```

`terra help` lists every command. The docs:

- [The CLI](https://www.terraverse.arclabs.it/docs/cli): every command, with [init](https://www.terraverse.arclabs.it/docs/cli/init), [dev](https://www.terraverse.arclabs.it/docs/cli/dev), [install](https://www.terraverse.arclabs.it/docs/cli/install) and [publish](https://www.terraverse.arclabs.it/docs/cli/publish)
- [Writing a mod](https://www.terraverse.arclabs.it/docs/mods): its [structure](https://www.terraverse.arclabs.it/docs/mods/structure), the API ([game](https://www.terraverse.arclabs.it/docs/mods/api/game), [mod](https://www.terraverse.arclabs.it/docs/mods/api/mod)) and [publishing](https://www.terraverse.arclabs.it/docs/mods/publish)
- [Running a server](https://www.terraverse.arclabs.it/docs/server): [setting one up](https://www.terraverse.arclabs.it/docs/server/setup) and [its mods](https://www.terraverse.arclabs.it/docs/server/mods)

They're written in `apps/site/content/docs/`: the API is designed there first.

## Layout

```
bin/terra.ts      the entry point
src/cli/          presentation: arguments in, output and an exit code out
  commands/       one file per command
  presentation/   where output goes, and each command's live view (on @arcforge/arcline)
src/terra/        the work: projects, prepare, dev, the engine, your account and the registry
src/runtime/      the sidecar that runs mods; its wire ops are in runtime/wire/ops.ts
tests/            bun test
```

## Developing

```bash
bun run typecheck
bun run lint        # oxlint
bun test
bun run gen         # after changing runtime/wire/ops.ts: writes the engine's sidecar/ops.h
bun run build       # a compiled terra in .output/bin
```

`terral` runs terra from this source against `.output/bin`'s engine. `bun check:mods`
(from `apps/game`) runs the sidecar in a headless server; `bun check:registry` runs
publishing against the ArcLabs staging backend.
