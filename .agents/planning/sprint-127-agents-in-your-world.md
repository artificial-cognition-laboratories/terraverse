# Sprint 127: agents in your world

Spawn an agent from inside the game and have it join the world you're playing. Two ways in, one
shape: a world axond runs (research, Fleet, `axon <agent> -c world://…`), or the world you opened
from the game, which axond **attaches** to. Either way an agent leases a body in it, as in sprint 126.

Built on sprint 125 (`running/`, the run secret's gate) and 126 (bodies, `axon.world/0`, the lab).

## Decided (2026-11-27)

1. **Native, not a mod.** Finding and running agents is the player's machine, not a world's rule:
   the shell talks to axond. What agents may do in a world is policy, later (`core:agents`).
2. **axond runs agents; the game only asks.** It lists, spawns and stops through the daemon's
   socket, and on leaving a world (or quitting) stops the agents it started and detaches.
3. **Every world on this machine looks the same from outside.** A world the game opens while signed
   in is already gated (hosted for friends); its gate also takes a run secret, and the game writes a
   `running/` record for it (`"by": "game"`), as terra does for its servers. Signed out there's no
   gate and no agents: the list says to sign in.
4. **`terra attach <terra://localhost:port> --world-contract`** speaks `axon.world/0` for a world
   it doesn't run: bodies placed and removed as `terra serve` does, `close` takes its bodies out and
   leaves the world up, and it ends when the world does.
5. **The lab attaches**: `labs.terraverse.attach({ address })` / `detach`, listed beside the worlds
   it serves; `worlds.at(terra://…)` finds an attached world too. Leases, `-c` and Fleet unchanged.
6. **axond boots an agent into a world address** (the daemon owns it; `-c` keeps the terminal's).
7. **The palette's `*` list is Agents**: yours, each idle, here, or elsewhere. Enter spawns into
   this world; on one that's here, dismisses it. A present agent is a player: `@` lists it, and you
   talk to it in chat. Management (a Library page) and a private conversation come later.

## Phases

1. **The game's world, listed** (engine). `online::Listed`: a run secret and the `running/`
   record, while the world is open; the hosting gate takes the secret. **Done when** `terral body`
   (or a headless body by hand) joins a world opened in the game.
2. **`terra attach`** (cli). The contract over a world it doesn't run. **Done when** an attach
   places a walking body in a world opened in the game, and `close` leaves the world running.
3. **The lab attaches; the daemon launches into a world** (axon). **Done when**
   `axon <agent> -c terra://localhost:<port>` works against a world opened in the game.
4. **The game's side.** `bridge/agents` (the daemon's socket: `agents.list`, `agents.spawn`,
   `agents.stop`), the `*` list, stop-and-detach on leaving. **Done when** an agent is picked in the
   palette, walks in, and goes when you leave.

## Built

**Phase 1 (2026-11-27): the game's world, listed.**
- `online::Listed` (`shared/platform/online/listed.{h,cpp}`): a run secret and `running/<port>.json`
  (terra's shape, `"by": "game"`, 0600 in 0700), written when a signed-in world opens, removed when it
  closes (only if still this process's). The hosting gate takes its secret (`Gate::Opts::local`).
  `Launch` carries the home's `running/`. TestListed.

**Phase 2 (2026-11-27): `terra attach`.**
- `world/sources.ts`: a contract's world is `served` (the engine started, stopped on close) or
  `attached` (a `running/` record; closing lets it go; it ends when the record's process does).
  `WorldContract` speaks for either; `terra serve --world-contract` uses `served`.
- `RunningRecord.by` ("terra" | "game"; absent reads as terra); `running.at(port)`.
- A body's `leave()` is idempotent (closing and the world ending both clear bodies).
- Seen: attached to a `terra serve` world, placed a body (it joined), `close` took it out, the world stayed up.

**Phase 3 (2026-11-27): the lab attaches; the daemon embarks agents** (arclabs, axond).
- `LabT.attach({ address })`, `RunningWorld.attached`; Minecraft and Terraria refuse it.
- `Served` runs `terra serve --world` or `terra attach <address>`; an attached world goes by the
  name it says. `Worlds.attach`: the world already on that port, or one attach at a time.
- `worlds.at(terra://…)` (the CLI's `-c`) attaches instead of refusing.
- `Launcher.launch(ref, { connect })` gives the agent `AXON_CONNECT`; `Embark` (the daemon's `embark`
  verb, on both roots): the world made ready (a game's address attached, a lab world started), then
  the agent launched into it, daemon-owned; stopped with `agents.stop(session)`.
- Seen (in-process Axond): attach, a body placed and joined, the world let go and still up; Terry
  embarked in 14 s. Its lease waits on the installed dev daemon being restarted onto this code
  (the agent leases over the daemon's socket).

**Phase 4 (2026-11-27): the game's side.**
- `shell::Axond` (`bridge/axond.{h,cpp}`): one verb over the daemon's unix socket (HTTP, chunked or
  not); a run-in-place build asks `axonl`'s daemon (~/.axon-dev), an installed game `axon`'s
  (~/.axon), `AXON_DAEMON_DIR` either. `Down` when nothing answers. TestAxond (a fake daemon).
- `shell::Agents` (`bridge/agents.{h,cpp}`), a member of the world's `Session`, given its address by
  the game when it's listed (signed in): `agents.list` (own installed + live, each idle, starting,
  here, elsewhere or failed), `agents.embark` (answered at once, booted on its own detached thread
  sharing state by `shared_ptr`; a boot that finishes after the world left is sent away),
  `agents.stop`. Gone with the world: its agents stopped, the lab's world let go (3 s each).
- `terra attach` ends when the world's record goes or changes hands, not only its process: the
  game outlives a world it leaves.
- The palette's `*` (`game/palette/agents.ts`): `AgentsFeed` (read on open, every second while one is
  starting) and the view; Enter brings one or sends it away; a folder or `@scope/name` typed whole is
  brought as named. The `*` key (Shift+8) opens it.
- Not yet seen in the game: needs the dev daemon on this code (`axonl daemon restart`).
