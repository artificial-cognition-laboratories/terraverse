# Sprint 126: Axon bodies

Agents play Terraverse. axond runs a Terraverse server as its one lab, and an agent leases a body
in it: a real game client with no window, sensing the world (pictures, IMU, labelled blocks) and
driving a player through the same movement code a person's does. You watch it from your own game.

Built on sprint 125: `terra serve`, `running/`, the run secret's gate.

## Decided (2026-11-24)

1. **Terraverse is the only lab, for now.** Atrium is deleted from arclabs (the app went in `rm
   atrium`; its lab, Fleet's views and the contract's name follow). The Minecraft and Terraria modules
   stay in the tree, out of `LAB_IDS`. The registry stays generic: another lab is one entry.
2. **The world contract is `axon.world/0`**, and `ready` names its engine generically
   (`engine: { name, version }`), not MuJoCo.
3. **`terra serve` speaks the world contract** on its own pipes (`--world-contract`); axond's
   `TerraverseLab` spawns it. One process, one world; when axond goes, so does the world.
4. **A body is a headless game client**: the engine's `--body` mode. It joins as a player
   through the run secret's gate and runs the real movement. No window (EGL offscreen), no
   Chromium, no sound, no far view; no meshing at all unless it was leased vision.
5. **terra hosts every body's bridge** (`axon.body/1` over WebSocket); a body process talks to
   terra over a pipe, as the sidecar does. The engine runs no WebSocket server.
6. **Pictures are PNGs in a ring of files in `/dev/shm`** (memory, not disk); a reading's `Ref` is
   a `file://` URI. Changed in phase 3: the mind's body module keeps the file a ref names and hands
   it to the model, which wants an encoded image anyway; 320×240 costs ~10 ms to write.
7. **The skeleton is the figure's**: its rig exported as URDF; the manifest's frames come from the
   same source.
8. **The lease chooses senses.** The manifest is the menu; `LeaseRequest` gains `senses?` (channels;
   presets are names that expand to channels) and a vision size. A body computes only what was
   leased. Absent: everything.
9. **Identity: the host's account.** A body joins under a local name (`<owner>.<agent>`), admitted
   by the run secret, owned by whoever runs the server. A throwaway ArcLabs account later is only a
   different name at join.
10. **Scenarios wait.** `snapshot`/`restore`/`reset` and stepped pace are NOT_WIRED until an
    agent needs them.

## Senses

| Tier | Channels |
|---|---|
| Raw (a player's) | `/vision`, `/depth`, `/imu`, `/contact`, `/posture`, `/interoception`, `/hearing` |
| Labelled | `/blocks` (voxels round the body, named, with their matter), `/entities`, `/position` (a `local` place, or `geodetic` on a planet) |
| Truth (recorded, never to the mind) | exact position, velocity |

Controls as the Minecraft body's, so agents port: `/legs`, `/look`, `/hands`.

## Handshake

```
mind ─lease {agent, senses, vision}─► axond (TerraverseLab)
axond ─place {body, name, token}─► terra serve        (axon.world/0)
terra: a warm body, or a new one; it joins by the run secret
axond ─Lease {endpoint: ws://127.0.0.1:<terra>/body/<name>, token}─► mind
mind ─connect ?token─► terra ─► Manifest (frames + URDF) ─► readings / commands
```

Release returns the body to the warm pool; its controls lapse as declared.

## Phases

1. **Contract and cleanup** (arclabs). `axon.world/0`, `ready.engine`; Atrium's lab, client verbs,
   Fleet views and tests gone; `LAB_IDS = ["terraverse"]`, Minecraft and Terraria unregistered;
   `TerraverseLab` listing server folders and starting `terra serve --world-contract`; terra's side:
   `ready`, `state`, `close`. **Done when** Fleet shows Terraverse, starts a server, and the game joins it.
2. **A blind body.** `--body` without rendering, the pipe, terra's bridge, manifest and URDF;
   `/imu`, `/contact`, `/position`, `/blocks`; `/legs`, `/look`, `/hands`. **Done when** an agent
   walks a body and you watch it from your client.
3. **Vision.** EGL offscreen, the shm ring, `/vision`, `/depth`. **Done when** measured: memory and
   fps at 10, then 50 bodies.
4. **Leased senses** in full, `/hearing`, a warm pool sized by the lab.

If 50 seeing bodies are too heavy, the next rung is one body host running many bodies over one
shared map and mesh cache: a real `Client` refactor, done only on the numbers.

## Built

**Phase 1 (2026-11-24).**
- arclabs: the world contract is `axon.world/0`, `ready.engine` replaces `mujoco`, and `NOT_WIRED` is a code.
  The geometry view protocol stays, for any lab's observer.
- axond: Atrium's lab became `labs/terraverse` (its world runner and bodies kept, its library and
  binary replaced by `Servers`, folders under `<axon>/worlds/terraverse/servers`, and `Terra`, found
  by AXON_TERRA or on PATH). `LAB_IDS = ["terraverse"]`; `LabKind` names every lab in the tree, so
  Minecraft and Terraria still typecheck, unregistered.
- Fleet: Terraverse's row and icon; Minecraft Studio, Atrium's library and import, Terraria's
  interventions panel and Minecraft's world and player controls removed. The scene viewer stays.
- terra: `terra serve --world-contract` (`src/terra/world`): `ready` once the engine listens,
  `state`, `close`, ticks; everything else answered NOT_WIRED. It writes its `running/` record, so
  the game lists it.
- Seen: TerraverseLab started a `terra server init` folder through terral (ready in ~0.6 s), refused
  a body NOT_WIRED, and stopped it with nothing left behind.

**Phase 2 (2026-11-24): a blind body.**
- Engine: `luanti --body --address --port --name` (`client/app/headless/`): `Headless` connects a
  `Client` on the NULL video driver through the run secret's gate and steps it at 60 Hz; `Motor`
  turns `/legs`, `/look`, `/hands` into a `PlayerControl` (held 250 ms, then let go), `/say` into
  chat; `Senses` reads `/imu` (the planet's own field), `/contact`, `/interoception`, `/position`,
  `/blocks`, `/chat`; `Link` is the pipe (stdout moved aside, fd 1 → stderr). The manifest's frames,
  senses and controls are declared in C++, one table each. A death respawns at once.
- Fixed on the way: the load screen draws nothing on the NULL driver (no fonts there);
  `Body::readout()` asked a character's velocity as an assembly's and threw.
- terra: `src/terra/body` (`Body`, `Bodies`, `Bridge`, the body wire, the contract's translation);
  `place`, `remove`, `state` and `close` act on real bodies; `lines` stops reading at the world's
  end (axond keeps stdin open: terra never exited).
- axond: a lease's body is `player` unless named; the name is cut to the 20 a player's may be.
- Seen: through TerraverseLab, a lease in ~4.4 s (a first join downloads content; ~1 s after),
  the token enforced (403 without), manifest and `embodied`, it walked, sprinted ~8 m in 3 s,
  turned exactly as commanded, spoke in chat, release and stop clean.
- Measured: a blind body is ~245 MB private (Pss ~258, RSS ~340 counting Chromium's shared pages)
  and ~10% of a core. By stage: 117 MB before anything (the binary), +71 MB preparing node textures
  and meshes, +40 MB entering, +90 MB in the first seconds (map blocks meshed, physics).

**Phase 3 (2026-11-24): vision.**
- `luanti --body --vision WxH`: SDL's offscreen driver (a GL context by EGL, no display), OpenGL 3.
  `Eye` (`client/app/headless/eye.h`) draws as the game does through `RenderingCore` (no HUD), the
  camera at the head, the map told where it is each step, a `Sky`; 4 a second it writes a PNG to a
  ring of 8 in `/dev/shm/terraverse-bodies/<name>-<pid>/` and sends `/vision` with its ref (removed
  when the body goes). The lens is a pinhole from the camera's vertical field of view.
- The world's shader uniforms moved out of `Game` (`render/shaders/world_uniforms.{h,cpp}`, ~300
  lines out of game.cpp) so the eye's shaders get what the game's do; the fog uniforms too.
- No loading screen, no HUD where there are no fonts (renderingengine.cpp, pipeline/plain.cpp).
- terra gives every body a 320×240 eye for now; readings carry a `ref`.
- Fixed: removing phase 2's memory trace left an `if` guarding the senses: every sense but vision
  read once in 3 s. Phase 2's reported numbers were taken before it.
- Seen: a body's pictures of the world (sky, grass, soil, a granite cliff), turning and sprinting
  through terra; blind bodies unchanged.
- Measured, one seeing body: ~424 MB private, ~49% of a core (meshing 96 nodes round it, drawing).

**After phase 3 (2026-11-24): the player's worlds, and terra:// addresses.**
- The lab's worlds are the player's Play list: `terra worlds --json` (the active profile of the
  home terra's build uses: a source axond runs `terral`, the dev home), each run as
  `terra serve --world <name> --world-contract`. The lab's own `servers/` folder is gone.
- One engine to a world: a server holds `<world>/world.lock` (flock; `server/app/world_lock.h`).
- `axonl <agent> -c terra://localhost:<port>`: the game's address, as the lab world serving there.
- Terry walks and turns (`cognet/src/control/wander.system.ts`, TEMP: names the controls).
- Seen: 81 worlds listed in the lab; Terry walking in Bramble Mesa; old worlds (TF6–TF23) are
  listed and refused on start by the engine's own reason, as the game refuses them.

**The figure as the skeleton, vision at 30 (2026-11-24).**
- Frames are the figure the game draws (`physics::eachBone`, one table the figure's boxes read too):
  each bone a link with its box, hung by three one-axis joints (`<bone>.z`, `.y`, `<bone>`: Irrlicht's
  Rz·Ry·Rx moved into flu, TestBodyRig checks it against the engine's matrices), an eye on the head.
  `/joints` (sensor_msgs/JointState) reports every joint's angle from the posed figure, 20 a second.
- Vision 30 a second, the ring 64 (~2 s for a mind to read a picture).

## Left

- `/depth`: not yet.
- The near world only: no far view, space or liquids in a body's eye; blocks not yet meshed show as
  black holes.
- A blind body's cost: skip node textures and map meshing when it has no vision (~160 MB of the
  245); then measure 10 and 50.
- A bone's place on its parent is its resting one: a crouch's lowered hips are said as turns only.
- Bodies' names: `<owner>-<agent>` was decided as `<owner>.<agent>`, but a player's name has no dot.

- Fleet's new views haven't been seen in VS Code; the daemon needs a restart to pick up the lab.
- Fleet can't make a server yet: `terra server init <name>` in the lab's `servers/` folder.
- The site's `terra serve` page lists `--json`, which `serve` refuses (it takes only --port and
  --world-contract).
