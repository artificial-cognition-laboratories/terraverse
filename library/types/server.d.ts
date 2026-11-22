// A mod's server half: server/main.ts and everything it imports.
export {}

declare global {
    // ── content ─────────────────────────────────────────────────────────────

    /** A file in the mod's assets/textures/, or a picture art.texture drew. */
    type Texture = TextureName | ArtTexture

    interface BlockDef {
        /** What it's made of: its weight, strength, grip and whether light passes come from it. Every block digs like stone for now. */
        matter: string
        /** 0–1: how much of the block is matter. Default 1. */
        share?: number
        /** What players see it called. Default: from its name, "kilns:granite_brick" → "Granite brick". */
        description?: string
        /** One texture on every face, or the top and bottom apart from the sides. */
        texture: Texture | { top?: Texture; bottom?: Texture; sides: Texture }
    }

    interface BlockType {
        readonly name: BlockName
    }

    interface Blocks {
        /** Declare a block. Load only. Throws NAME_TAKEN, NAME_NOT_YOURS, ASSET_MISSING, CONTENT_SEALED. */
        create(name: OwnName, def: BlockDef): BlockType
    }

    interface ItemDef {
        /** What players see it called. Default: from its name. */
        description?: string
        /** Shown in the inventory, and extruded into a thick sprite in the hand: clear pixels are its outline. */
        icon: Texture
    }

    interface ItemType {
        readonly name: BlockName
    }

    interface Items {
        /** Declare a hand-sized thing. Load only. It does nothing yet but exist, show and stack. Throws NAME_TAKEN, NAME_NOT_YOURS, ASSET_MISSING, CONTENT_SEALED. */
        create(name: OwnName, def: ItemDef): ItemType
    }

    interface ContentApi {
        readonly blocks: Blocks
        readonly items: Items
    }

    /** What things are. Declared while mods load, sealed after. */
    const content: ContentApi

    // ── art ─────────────────────────────────────────────────────────────────

    /** #rgb, #rrggbb or #rrggbbaa; or, inside a texture with a palette, one of its letters. */
    type ArtColor = `#${string}` | string

    /**
     * One pass of paint, over what's already there. Noise and grain wrap at the edges, so
     * a block's faces tile; every seed gives the same picture on every machine.
     */
    type ArtLayer =
        /** Every pixel this colour. */
        | { fill: ArtColor }
        /** Blotches (stone, dirt): colours darkest first, a blob every `scale` pixels (default 4). */
        | { noise: readonly ArtColor[]; scale?: number; seed?: number }
        /** Streaks (wood): along "y" runs them top to bottom (default), "x" side to side. */
        | { grain: readonly ArtColor[]; along?: "x" | "y"; seed?: number }
        /** Single pixels scattered: `density` 0–1 of them (default 0.1). */
        | { speck: ArtColor; density?: number; seed?: number }
        /** A grid in the palette's letters, one string per row; "." and " " leave what's under. */
        | { rows: readonly string[] }
        /** A frame round the edge, `width` pixels (default 1). */
        | { border: ArtColor; width?: number }
        /** The light, top-left: edges facing it lightened, edges facing away darkened, by 0–1. Shapes as well as squares. */
        | { bevel: number }
        /** A line round whatever's drawn, on the clear pixels beside it: an item's silhouette. */
        | { outline: ArtColor }

    interface ArtTextureDef {
        /** 16 (default) or 32 pixels square. */
        size?: 16 | 32
        /** One character to a colour. "." and " " are always clear. */
        palette?: Record<string, ArtColor>
        /** Painted in order. */
        layers?: readonly ArtLayer[]
        /** A last layer of rows, on top: `{ palette, rows }` alone is a whole picture. */
        rows?: readonly string[]
    }

    /** A picture art drew, for a block's texture or an item's icon. */
    interface ArtTexture {
        readonly size: number
    }

    interface ArtApi {
        /** Paint a texture from a definition. Load only. Throws ART_INVALID, saying where. */
        texture(def: ArtTextureDef): ArtTexture
    }

    /** Pictures drawn from code: no image files to make. `terra art render` shows them. */
    const art: ArtApi

    // ── game ────────────────────────────────────────────────────────────────

    interface Player {
        /** "@cody": unique and permanent. */
        readonly name: AccountName
        /** False once they've left; then everything else throws PLAYER_GONE. */
        readonly connected: boolean
        /** A line in their chat, from the server. */
        tell(message: string): void
        /** Whether they hold a permission. Admins (the engine's `server` privilege) hold every one; roles don't grant any yet. */
        can(permission: PermissionName): boolean
        /** Disconnect them, with a reason they're shown. They can come back. */
        kick(reason: string): void
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

    type CommandArg =
        | { type: "string"; optional?: boolean; options?: readonly string[] }
        | { type: "number" | "integer"; optional?: boolean; min?: number; max?: number }
        | { type: "boolean"; optional?: boolean }
        | { type: "player" | "position"; optional?: boolean }

    type CommandArgs = Record<string, CommandArg>

    /** What each argument arrives as: a "player" as a Player, a "position" as a Vec3. */
    type ParsedArg<A extends CommandArg> =
        A extends { type: "string"; options: readonly (infer O)[] } ? O :
        A extends { type: "string" } ? string :
        A extends { type: "number" | "integer" } ? number :
        A extends { type: "boolean" } ? boolean :
        A extends { type: "player" } ? Player :
        A extends { type: "position" } ? Vec3 : never

    type ParsedArgs<A extends CommandArgs> = {
        [K in keyof A]: A[K] extends { optional: true } ? ParsedArg<A[K]> | undefined : ParsedArg<A[K]>
    }

    interface CommandDef<A extends CommandArgs> {
        /** Shown in chat's /help. */
        description: string
        /** In the order they're typed. The last, if a string, takes the rest of the line. */
        args?: A
        /** Who may run it. Default: everyone. The server console may run anything. */
        permission?: PermissionName
        run(e: {
            /** undefined from the server console */
            player?: Player
            args: ParsedArgs<A>
            /** To whoever ran it, wherever they are. */
            reply(text: string): void
        }): void | Promise<void>
    }

    interface Commands {
        /** "kiln light" or ["kiln", "light"]: typed /kiln light. Throws COMMAND_TAKEN. */
        create<const A extends CommandArgs = {}>(path: string | readonly string[], def: CommandDef<A>): Disposer
    }

    interface GameApi {
        readonly players: Players
        /** What players type in chat. */
        readonly commands: Commands
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
