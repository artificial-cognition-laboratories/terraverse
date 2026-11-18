---
title: player
description: "The one player this client half runs for. Where they are, what they carry, their camera, what their body senses, and, for an agent, the controls. Read-only: only what the server has sent them."
---

# player

You: the one player this client runs for. Read-only, and only what the server has sent them.

```ts
interface PlayerApi {
    readonly name: AccountName       // "@cody"
    readonly world: { name: WorldName; planet: Planet }
    readonly position: Vec3
    readonly looking: Vec3           // the direction they face, length 1

    // The block they're looking at, within reach, or null.
    target(): { pos: Vec3; block: BlockName } | null

    // Landed on another world.
    on(event: "arrived", fn: (world: { name: WorldName }) => void): Disposer

    readonly camera: Camera          // their eyes: what fills the screen
    readonly sensors: Sensors        // what their body perceives, as data
    readonly controls?: Controls     // only on an agent's client. see controls

    readonly inventory: InventoryView   // what they carry, read-only. not designed yet
}
```

Here, unlike the server half, there is a "you". There's exactly one, and nobody else is visible
through this: other players are what the server chooses to send, as it sends everything else.

```ts
game.input.bind("kilns:bellows", { key: "B", label: "Work the bellows" }, () => {
    if (player.target()?.block !== "kilns:kiln") return
    server.send("bellows")
})
```

That check is a courtesy, to save asking for something that can't happen. The server checks
again, because a client's word is never enough.

## inventory

What they carry, as the server last sent it: read-only, like everything here. Changing it is the
server's business, through [Player](/docs/mods/api/game/players). Its shape comes with items and
inventories, which are measured by mass rather than counted: not designed yet.

## One client, one player

A client runs for exactly one player, and a player has one client at a time. Signing in from a
second machine takes over from the first. Someone watching rather than playing is still a player,
with a role that lets them touch nothing. Some clients have no window: an agent that sees in
pixels renders like anyone else, offscreen.

## camera

The player's eyes: the view that fills their screen.

```ts
interface Camera {
    readonly fov: number             // degrees: the player's setting, not yours to change
    shake(e: { strength: number; seconds?: number; at?: Vec3 }): void   // fades with distance from `at`
}
```

```ts
server.on("crack", ({ pos }) => player.camera.shake({ strength: 0.4, at: pos }))
```

There's one, and it's theirs. Views from anywhere else, a monitor showing a security camera, a
mirror on a cart, are [extra cameras](/docs/mods/api/client/game#cameras), drawn into a panel,
never in place of this one.

## sensors

What this player's body perceives, as data rather than pixels on a screen. A person mostly has no
use for it. An agent lives by it.

```ts
interface Sensors {
    // The sensors this body is fitted with: chosen by the server, never by the client.
    list(): readonly Sensor[]

    // Each new reading, at the sensor's rate.
    on<S extends SensorName>(sensor: S, fn: (reading: Reading<S>) => void): Disposer
}

interface Sensor {
    readonly name: SensorName
    readonly kind: SensorKind
    readonly rate: number            // readings per second
}

type SensorKind =
    // raw: what a body could physically measure
    | "camera.rgb" | "camera.depth"  // rendered, so the client needs a GPU
    | "imu"                          // acceleration and rotation, from the physics
    | "joints"                       // the body's own pose
    | "audio"
    // egocentric: what's in view, labelled, and nothing hidden behind it
    | "camera.labels"                // per pixel: which block, which matter, which body
    | "sight"                        // what's in view, as a list: names, positions, states
    // privileged: ground truth, for when perception isn't what's being tested
    | "surroundings"                 // every block and body within a radius, exactly
```

### What a body may know

Perception comes in three kinds, from what a real body could measure to what only the simulation
knows:

| Kind | Gives | Costs |
|---|---|---|
| **Raw** | Pixels, depth, IMU, the body's pose, sound | A renderer, for cameras |
| **Egocentric** | What's in view, labelled: blocks, matter, bodies. Nothing behind a wall | Ray casts, no GPU |
| **Privileged** | The exact state of everything nearby | Next to nothing |

An experiment that doesn't need pixels doesn't pay for a renderer. And because the sensors a body
has are **declared by the server**, by the scenario's rules, two runs of the same published server
perceive the same things: a result says what the agent could know, not just what it did.

Sensors belong to bodies, not to players. A rover built from parts can carry a camera part and an
IMU part, and perceives through them exactly as a player's body does. Declaring sensors, and
fitting them to bodies, is not designed yet.

## Agents

An agent joins as a player: an account, a body, a role, the same rules. It connects through a
client like anyone else, with or without a window, reads the world through `sensors`, and acts
through `controls`.

`controls` exists only on a client the server has let in as an agent, and is `undefined` on every
other. A mod can't drive a person's player: automating someone's hands is cheating, whoever's code
does it.

## controls

How an agent acts, at whichever level the scenario allows.

```ts
interface Controls {
    readonly levels: readonly ControlLevel[]   // what the server allows this agent
    readonly input?: InputControls             // undefined unless "input" is allowed
    readonly motor?: MotorControls
    readonly skills?: Skills
}

type ControlLevel = "input" | "motor" | "skills"
```

| Level | Is | Like |
|---|---|---|
| **input** | Keys, mouse and buttons, frame by frame | A person's hands |
| **motor** | Movements: move, look, jump, use, dig, place | What a controller maps to |
| **skills** | Tasks that take time and can fail: walk somewhere, dig a block out | What you'd ask a person to do |

### Each level is built from the one below

A skill acts through motor controls, and a motor control does what keys would. So no level can do
anything a person couldn't, and an agent working in skills is still subject to every rule, every
panel and every check a person is.

The same holds for knowledge. A skill sees only what the body's [sensors](#sensors) give it: a
skill that finds diamonds can't look through rock unless the scenario gave the body privileged
sensors. Which levels an agent may use is declared by the server, with its sensors, so a result
says both what the agent could know and how it could act.

### input

```ts
interface InputControls {
    // The state of the controls for the next frame, exactly as hands would leave them.
    frame(state: { keys?: readonly Key[]; mouse?: { dx: number; dy: number; buttons?: readonly MouseButton[] } }): void
}
```

### motor

```ts
interface MotorControls {
    move(direction: Vec3, e?: { speed?: number }): void   // walk, as the keys would. stop with [0, 0, 0]
    look(direction: Vec3): void
    jump(): void
    use(): void                      // what right-click does: open, operate
    dig(): void                      // start breaking what's in reach; stops when it breaks or you look away
    place(): void
}
```

### skills

```ts
interface Skills {
    // Every skill this agent can run: the engine's, and any it defined.
    list(): readonly SkillInfo[]

    // Start one. It acts through motor controls until it's done, fails, or is cancelled.
    run<S extends SkillName>(skill: S, args: SkillArgs<S>, e?: { signal?: AbortSignal }): Task<SkillResult<S>>

    // A new skill, built from motor controls, sensors and other skills.
    define<A, R>(name: OwnName, def: SkillDef<A, R>): Disposer
}

interface Task<R> extends Promise<R> {
    on(event: "progress", fn: (e: { done: number; note?: string }) => void): Disposer
}

interface SkillDef<A, R> {
    description: string
    run(e: { args: A; motor: MotorControls; sensors: Sensors; skills: Skills; signal: AbortSignal }): Promise<R>
}
```

```ts
const task = player.controls.skills.run("core:walk_to", { to: [120, 64, -40] })
task.on("progress", ({ done }) => log(done))
await task   // or throws Refused: "No way through: the river is too deep."
```

A skill fails like anything else in Terraverse: with a reason. The engine's own skills, `walk_to`,
`look_at`, `dig_out`, `place_at`, `use_at`, are the floor. Anything above that, mining a seam,
building a wall, finding diamonds, is a skill someone defined: in your agent, in a package on the
registry, or as an Axon agent that thinks.

Skills don't need permission from the scenario to be *defined*, only to be *run*: they're code
on the agent's side, acting through controls the scenario already allowed.

### People and agents, one surface

An agent can do exactly what a person could, through the same panels and the same checks, and
nothing else. A kiln mod written for people works for agents unchanged: an agent reads the kiln
panel's props and presses Stoke by sending its event.

The readings and controls follow the protocols Axon already uses for its agents, so a Fleet agent
plugs into a body here as it would into any other environment.
