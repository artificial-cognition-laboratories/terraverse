# Sprint 125: local servers

Make a server folder, run it, and join it from the game: `terra server init` → `terra serve`
(or `terra dev --headless`) → it shows in the Play list as running on this machine → join.
This is the ground Axon stands on later: Fleet spawns a server, agents and the player join the same
world. Axon itself isn't in this sprint.

## Addresses: `terra://`

Every place you can go has one address, a URI scheme (not a new protocol: the wire stays the
engine's). The ref grammar `terra <ref>` already has, given a prefix:

| Ref | Means | Resolved by |
|---|---|---|
| `terra://localhost:30000` | a server at an address | connect directly |
| `terra://TF9.dGVycmE` | a clean universe from a code | opened locally, alone |
| `terra://@cody/kilns` | a published server | the registry (later: NOT_WIRED) |
| `terra://friend/cody` | a friend's open world | ArcLabs presence, ticket, punch (later) |

- Parsed once at the boundary into a strict union, in TypeScript (cli) and C++ (the bridge), both
  against one golden table.
- No secret ever rides in a ref. A link from outside the game never joins without asking.
- Registering the scheme with the OS is the launcher's, later.

## Running servers: `running/`

`terra` writes `<profile>/running/<server>.json` when it starts an engine, removes it on exit:
`{ ref, title, mods, port, pid, secret, owner }`, readable only by the user. The game lists them
(dropping dead pids). No network discovery: LAN broadcast later is another source for the same list.

**Admission.** A dedicated server today refuses everyone (no gate). It gets a local gate: a client
presenting the run's secret is admitted under its name; the owner (terra's signed-in account) is
admin. It binds 127.0.0.1 unless `--public`, which is NOT_WIRED until ArcLabs issues tickets for
dedicated servers.

## Phases

Each phase ends with something run and seen, said plainly.

### 1. The starting point
- Smoke test: `terral server init`, `terral serve`, `terral dev --headless`; record what boots,
  what refuses, what's broken.
- Fix `server init`'s placement (decided below) and its next-step hint (it says `terra serve` when
  run as `terral`; the hint names the command as it was invoked).
- **Done when:** a fresh server folder boots headless with its mods, and the refusal is the known one.

### 2. Refs
- `apps/cli`: a `ref` module (parse, print), `terra <ref>` and `PlayCommand` on it; `terra://`
  accepted wherever a ref is.
- C++: the same parser in `shared/platform/online/`, one golden table shared by both test suites.
- **Done when:** both parsers pass the table; `terral terra://TF9…` opens a code.

### 3. Admission
- `online::` gets a local gate (a leaf beside `Gate`), one call from the server's start-up.
- The engine takes `--local-secret` (or a file) and `--owner`; binds loopback by default.
- terra makes the secret, writes and removes `running/` (`host` and `dev` both, crash-safe: a dead
  pid is ignored), prints `Join terra://localhost:<port>`.
- Protocol bumped if the hello changes.
- **Done when:** a headless probe client with the secret is admitted, without it refused with a reason.

### 4. Joining
- `Launch::Kind::Local`: connect to the address, the secret from the matching `running/` record
  as the ticket; no STUN, no punch. Without a record it's an ordinary server (refused for now).
- Bridge: `servers.running` (pushed as it changes) and `join { ref }`.
- **Done when:** the real game joins a `terra serve` server by verb, two profiles at once.

### 5. The Play list
- A "Running on this machine" group in `menu/play`: title, mods, ref, Join. A "Join by address"
  box taking a ref. Built from the kit.
- **Done when:** seen in the game: init, host, the entry appears, join, leave, stop, it's gone.

### 6. The check
- `check:serve`: init → host → probe joins with the secret → refused without → stop → record gone.
- AGENTS.md: the repo map's stale paths (`ui/src` → `src/client/app/shell/interface`,
  `src/online` → `src/shared/platform/online`), the scheme, `running/`.

## Decided

1. `terra://`, not `worlds://`: matches the CLI, unlikely to collide. Refs name the place, host:port
   is one kind (2026-11-24).
2. Discovery is a file per running server, not a broadcast (2026-11-24).
3. Axon waits for this sprint (2026-11-24): one engine, Terraverse, contract `axon.world/0`; Atrium
   deleted; the Minecraft and Terraria modules stay out of `LAB_IDS`.

4. `init` makes a bare name where you are (2026-11-24).
5. Local servers show in the Play list's Online section, tagged Local with their ref; no new
   list (2026-11-24).
6. The game's own pages moved from `terra://` to `shell://`: terra:// is a place's address (2026-11-24).

## Built (2026-11-24)

- Launcher: writes the `terra`/`terraverse` shims in ~/.local/bin on every start (shims.rs), so a
  command added after install reaches older installs.
- terra: refs (`src/terra/ref`), `running/` records (`src/terra/running`), the run secret and owner
  passed to the engine by environment, `Join terra://…` printed; SIGTERM/SIGHUP passed on to the
  engine (terra stopped alone orphaned it).
- Engine: `online::ref`, `online::LocalRun` (the gate admits by the run secret, loopback only),
  `LocalServers` (bridge), `servers.list` carries `local`, `servers.join { ref }` in the menu.
- Client: refused joins no longer abort on the net trace (client.cpp, getLocalStat with no peer).
- Checks: TestRef, TestOnline.testLocalGate, terra's ref/running tests, `check:serve`; `check:dev`'s
  stale log pattern fixed.

## Left

- Library's Servers screen doesn't list local servers yet (the Play list does).
- Joining from terra (`terra terra://localhost:30000`) is NOT_WIRED.
- `--public` and dedicated servers admitting by ArcLabs ticket (needs ../arclabs).
- `check:dev` uses the real home's cache (no TERRAVERSE_HOME), unlike `check:serve`.
