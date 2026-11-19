// The wire between the engine and the sidecar, declared once. `bun run gen` writes
// apps/game/src/sidecar/ops.h from this file, so the two sides can't drift.
//
// A frame is a u32 (little endian) length, then a MessagePack array [op, id, payload].
// id is 0 when no answer is wanted.

export const PROTOCOL = 2

// What a frame is.
export const Op = {
    // sidecar → engine, once, after every mod's server half has loaded:
    // { protocol, blocks: BlockDef[], media: { name, path }[] }
    content: 1,
    // engine → sidecar: open a window. { now, dt, events: Event[] }
    step: 2,
    // sidecar → engine: the window is done.
    idle: 3,
    // sidecar → engine: [Call, ...args]. Answered with reply when id isn't 0.
    call: 4,
    // engine → sidecar: [true, value] or [false, reason]
    reply: 5,
    // engine → sidecar: stop now. The sidecar exits without answering.
    stop: 6,
    // sidecar → engine: [Level, mod or "", text]
    log: 7,
} as const

// What the engine can be asked inside a window.
export const Call = {
    "player.tell": 1,       // name, text
    "game.announce": 2,     // text
    "players.list": 3,      // → names
    "commands.set": 4,      // [[name, description, params], …]: every command's first word, the whole set
    "player.can": 5,        // name, permission → whether they may
    "player.kick": 6,       // name, reason
} as const

// What happened since the last window.
export const Event = {
    join: 1,                // [Event.join, name]
    leave: 2,               // [Event.leave, name]
    command: 3,             // [Event.command, name ("" from the console), command, the rest of the line]
} as const

/**
 * How the engine's process ends, beyond 0 and 1. `restart`: the mods now declare different
 * content than the universe started with, so it stopped to be started again (terra dev and
 * terra host do), rather than run on with content that doesn't match.
 */
export const Exit = {
    restart: 75,
} as const

export const Level = {
    info: 1,
    warn: 2,
    error: 3,
} as const
