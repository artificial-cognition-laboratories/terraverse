// A mod's server half: server/main.ts and everything it imports.
export {}

declare global {
    // ── content ─────────────────────────────────────────────────────────────

    interface BlockDef {
        /** What it's made of. Not simulated yet: blocks are drawn, and dig like stone. */
        matter: string
        /** 0–1: how much of the block is matter. Default 1. */
        share?: number
        texture: TextureName | { top?: TextureName; bottom?: TextureName; sides: TextureName }
    }

    interface BlockType {
        readonly name: BlockName
    }

    interface Blocks {
        /** Declare a block. Load only. Throws NAME_TAKEN, NAME_NOT_YOURS, ASSET_MISSING, CONTENT_SEALED. */
        create(name: OwnName, def: BlockDef): BlockType
    }

    interface ContentApi {
        readonly blocks: Blocks
    }

    /** What things are. Declared while mods load, sealed after. */
    const content: ContentApi

    // ── game ────────────────────────────────────────────────────────────────

    interface Player {
        /** "@cody": unique and permanent. */
        readonly name: AccountName
        /** False once they've left; then everything else throws PLAYER_GONE. */
        readonly connected: boolean
        /** A line in their chat, from the server. */
        tell(message: string): void
    }

    interface Players {
        on(event: "join" | "leave", fn: (player: Player) => void | Promise<void>): Disposer
        /** Everyone connected now. */
        list(): readonly Player[]
        /** Someone connected now, by account. */
        get(name: AccountName): Player | undefined
    }

    interface Time {
        /** Seconds of game time since the universe began. */
        readonly now: number
        /** In game time, for this run: gone if the server restarts. */
        after(d: Duration, fn: () => void | Promise<void>): Disposer
        every(d: Duration, fn: () => void | Promise<void>): Disposer
        wait(d: Duration): Promise<void>
    }

    interface GameApi {
        readonly players: Players
        readonly time: Time
        /** A line to everyone connected. */
        announce(message: string): void
    }

    /** What's being played: its people and its clock. */
    const game: GameApi

    // ── mod ─────────────────────────────────────────────────────────────────

    /** This mod's settings, typed from its mod.config.ts (prepare writes them into settings.d.ts). */
    interface ModSettings {}

    interface ModApi {
        /** "@you/kilns" */
        readonly name: string
        /** "kilns": what everything it creates is named under. */
        readonly prefix: string
        readonly version: string
        /** The server's choices, checked against what this mod declares, every default filled in. */
        readonly settings: Readonly<ModSettings>
    }

    /** This mod: who it is, and what the server chose for it. In each mod's code, its own. */
    const mod: ModApi
}
