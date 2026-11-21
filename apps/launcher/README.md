# Launcher

The desktop app that installs, updates and starts the game. Players get it from
[terraverse.arclabs.it](https://www.terraverse.arclabs.it/); [installing](https://www.terraverse.arclabs.it/docs/install) covers it. Tauri: a Vue UI in `src/`, a
Rust backend in `src-tauri/`.

```
src/              the UI: pages, parts, and the store
  backend/        the one door to the backend: tauri.ts for real, mock.ts for the UI alone
src-tauri/        the Rust side: installing, versions, launching
scripts/          release.ts
```

## Developing

```bash
bun run dev         # the app (or `bun dev:launcher` from the root)
bun run dev:mock    # the app against the mock backend: nothing is installed
bun run ui          # the UI alone in a browser, on :5175
bun run typecheck
bun run lint        # oxlint, on the UI
bun run lint:rust   # cargo clippy, warnings denied
```

## Releasing

The launcher releases on its own track, apart from the game: `arc deploy launcher`. Its
version is `src-tauri/tauri.conf.json`'s (Cargo.toml must agree). `release:gate` checks the
versions, the types and `cargo test`; `release:tag` pushes a `launcher-v<version>` tag, which
CI builds and publishes; `release:wait` waits until the updater points at it.
