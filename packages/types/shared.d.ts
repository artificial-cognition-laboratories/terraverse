// Every file in a project, on either half or neither.
export {}

declare global {
    /** A block, in its world. y is up. */
    type Vec3 = readonly [x: number, y: number, z: number]
    type Duration = { seconds?: number; minutes?: number; hours?: number }
    /** Returned by everything that registers: call it, and the registration is gone. */
    type Disposer = () => void

    type BlockName = `${string}:${string}`
    /**
     * `prefix:name`: what a mod may create, under its own prefix (checked when it runs).
     * Broad on purpose: a mod imported by another is checked under the importer's types,
     * so one mod's own names can't be global until mods ship their own declarations.
     */
    type OwnName = `${string}:${string}`
    /** A file in the mod's assets/textures/ (checked when it runs). Broad, as OwnName is. */
    type TextureName = string
    /** An ArcLabs account: "@cody". */
    type AccountName = string

    /** Lines for the server's log, under your mod's name. */
    const console: {
        log(...values: unknown[]): void
        info(...values: unknown[]): void
        debug(...values: unknown[]): void
        warn(...values: unknown[]): void
        error(...values: unknown[]): void
    }

    /** Throw from any handler to stop the action. The player sees the reason. */
    class Refused extends Error {
        constructor(reason: string)
    }
}
