import { parseStack, type AxonErrorLike } from "@arcforge/err"

/**
 * Every failure terra can name. One flat map, one entry per failure: a code is a stable
 * identity a player, a modder or a support thread can refer to, so codes are never reused
 * or renamed once shipped. A new failure gets an entry here before its call site exists.
 *
 * `title` is the headline; `description` is a sentence or two for someone who has never
 * read this code. `expected` marks a failure the user caused and can fix: it renders
 * without our stack, because the title already says what to do.
 *
 * Mods see the runtime's codes (NAME_TAKEN, PLAYER_GONE…) as `error.code`, and the docs
 * name them, so those keys are part of the mod API.
 */
const MAP = {
    // ── using terra ────────────────────────────────────────────────────────
    USAGE: {
        title: "That's not how this command is used",
        description: "An argument is missing or isn't one the command takes. `terra help` lists every command.",
        source: "cli", severity: "fatal", expected: true,
    },
    COMMAND_UNKNOWN: {
        title: "No such command",
        description: "terra doesn't have that command. `terra help` lists every command.",
        source: "cli", severity: "fatal", expected: true,
    },
    NOT_WIRED: {
        title: "Not built yet",
        description: "This is designed but not implemented yet.",
        source: "cli", severity: "fatal", expected: true,
    },
    UNEXPECTED: {
        title: "Something went wrong inside terra",
        description: "A failure terra has no name for: this is a bug in terra, not in your server or mods.",
        source: "cli", severity: "fatal",
    },

    // ── server folders ─────────────────────────────────────────────────────
    PROJECT_NOT_FOUND: {
        title: "Not in a project",
        description: "A project is a folder with a package.json and its kind's config: server.config.ts for a server, mod.config.ts for a mod. Run this inside one, or name one.",
        source: "manifest", severity: "fatal", expected: true,
    },
    PROJECT_EXISTS: {
        title: "That folder isn't empty",
        description: "A new project is made in a new or empty folder: init never writes over files.",
        source: "manifest", severity: "fatal", expected: true,
    },
    PROJECT_WRONG_KIND: {
        title: "Not that kind of project",
        description: "This command works on one kind of project, and the nearest one is another.",
        source: "manifest", severity: "fatal", expected: true,
    },
    PACKAGE_MISSING: {
        title: "package.json is missing",
        description: "Every server and every mod has a package.json: its name, its version and what it builds on.",
        source: "manifest", severity: "fatal", expected: true,
    },
    PACKAGE_INVALID: {
        title: "package.json can't be read",
        description: "It needs to be JSON with a name, a version and its dependencies.",
        source: "manifest", severity: "fatal", expected: true,
    },
    CONFIG_INVALID: {
        title: "A config file can't be read",
        description: "mod.config.ts must `export default defineMod({ … })`, and server.config.ts `defineServer({ … })`.",
        source: "manifest", severity: "fatal", expected: true,
    },
    SETTING_INVALID: {
        title: "A setting's value isn't one it takes",
        description: "Each setting a mod declares says what it takes: a number in its range, true or false, one of its options, or text that fits its pattern.",
        source: "manifest", severity: "fatal", expected: true,
    },
    SETTING_UNKNOWN: {
        title: "No such setting",
        description: "A server can only choose the settings its mods declare in their mod.config.ts.",
        source: "manifest", severity: "fatal", expected: true,
    },
    NOT_INSTALLED: {
        title: "A dependency isn't installed",
        description: "It's in package.json but not in node_modules.",
        source: "manifest", severity: "fatal", expected: true,
    },
    INSTALL_FAILED: {
        title: "Installing failed",
        description: "bun install didn't finish. What it said is below.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MOD_NAME_INVALID: {
        title: "A mod's name can't be a prefix",
        description: "A mod is @scope/lower-case: the part after the slash names everything it creates.",
        source: "manifest", severity: "fatal", expected: true,
    },
    PREFIX_TAKEN: {
        title: "Two mods share a prefix",
        description: "Each would name the other's blocks. Only one of them can run on a server.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MOD_MISSING: {
        title: "A mod builds on one this server doesn't run",
        description: "Every mod that runs is in the server's package.json, including the ones other mods build on.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MOD_CYCLE: {
        title: "Mods build on each other in a circle",
        description: "A mod loads after the mods it builds on, so a circle can't load at all.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MOD_UNDECLARED: {
        title: "A mod imports one it doesn't depend on",
        description: "Add it to the importing mod's package.json dependencies, so it loads first.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MOD_SUBPATH: {
        title: "A mod is imported by its name",
        description: "Import what a mod exports from its name alone, not a file inside it.",
        source: "manifest", severity: "fatal", expected: true,
    },
    BUILD_FAILED: {
        title: "A mod didn't build",
        description: "Its server half couldn't be bundled. What the bundler said is below.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MANIFEST_MISSING: {
        title: "This server isn't prepared",
        description: ".terra/manifest.json is missing. `terra prepare` makes it.",
        source: "manifest", severity: "fatal", expected: true,
    },
    MANIFEST_INVALID: {
        title: ".terra/manifest.json can't be read",
        description: "It was made by a different terra, or edited. `terra prepare` makes it again.",
        source: "manifest", severity: "fatal", expected: true,
    },
    ENGINE_MISSING: {
        title: "No engine beside terra",
        description: "terra runs the engine installed beside it. Run it from the built or installed game.",
        source: "cli", severity: "fatal", expected: true,
    },

    // ── the sidecar ────────────────────────────────────────────────────────
    PROTOCOL: {
        title: "The engine and the sidecar disagree",
        description: "A frame on the wire wasn't what the protocol says. The engine and terra may be from different builds.",
        source: "runtime", severity: "fatal",
    },
    ENGINE_GONE: {
        title: "The engine went away",
        description: "Its end of the socket closed without telling the mods to stop: it crashed or was killed.",
        source: "runtime", severity: "fatal", expected: true,
    },
    MOD_LOAD_FAILED: {
        title: "A mod failed to load",
        description: "Its server half threw while loading, so the universe can't start. Its error is in the server's log.",
        source: "runtime", severity: "fatal", expected: true,
    },

    // ── the mod API: what a mod's code can catch ───────────────────────────
    NOT_RUNNING: {
        title: "The universe isn't running yet",
        description: "Mods are loading: there's no world to read and nobody to talk to until they all have.",
        source: "runtime", severity: "fatal", expected: true,
    },
    CONTENT_SEALED: {
        title: "Content is sealed",
        description: "Blocks are declared while mods load. Once every mod has loaded, nothing new can be declared.",
        source: "runtime", severity: "fatal", expected: true,
    },
    NAME_INVALID: {
        title: "Not a name",
        description: "A name is prefix:lower_case.",
        source: "runtime", severity: "fatal", expected: true,
    },
    NAME_NOT_YOURS: {
        title: "Not your name",
        description: "A mod only creates things under its own prefix.",
        source: "runtime", severity: "fatal", expected: true,
    },
    COMMAND_TAKEN: {
        title: "That command is taken",
        description: "Another mod, or this one, already has a command at that path.",
        source: "runtime", severity: "fatal", expected: true,
    },
    NAME_TAKEN: {
        title: "Already declared",
        description: "Something with that name already exists.",
        source: "runtime", severity: "fatal", expected: true,
    },
    MATTER_UNKNOWN: {
        title: "Made of what?",
        description: "Every block says what it's made of: a substance or a mixture.",
        source: "runtime", severity: "fatal", expected: true,
    },
    ASSET_MISSING: {
        title: "An asset is missing",
        description: "A texture is a file in the mod's assets/textures/.",
        source: "runtime", severity: "fatal", expected: true,
    },
    ART_INVALID: {
        title: "That art can't be drawn",
        description: "A texture is a square grid (16 or 32), each row as long as it's tall, every letter in its palette, and colours written #rgb, #rrggbb or #rrggbbaa. The detail says where.",
        source: "runtime", severity: "fatal", expected: true,
    },
    TYPES_FAILED: {
        title: "The mod doesn't typecheck",
        description: "TypeScript found mistakes in the mod's code: each is listed with its file and line. The types are the mod API's: .terra/types/server.d.ts says what each verb takes.",
        source: "runtime", severity: "fatal", expected: true,
    },
    BLOCK_UNKNOWN: {
        title: "No such block in the game",
        description: "--with names the game's own blocks (stone, dirt, oak_log…), as assets/world/blocks.json declares them. The detail lists them.",
        source: "runtime", severity: "fatal", expected: true,
    },
    ARGUMENT_INVALID: {
        title: "An argument isn't what this takes",
        description: "A value passed to the API has the wrong shape or is out of range.",
        source: "runtime", severity: "fatal", expected: true,
    },
    PLAYER_GONE: {
        title: "That player has left",
        description: "A Player outlives their connection. Check `player.connected` after an await.",
        source: "runtime", severity: "fatal", expected: true,
    },
    NOT_PLACED: {
        title: "Not anywhere yet",
        description: "A player who has just joined has no body in the world until they're placed, and a position on a seam between the planet's tiles is on no world. Try again in a moment.",
        source: "runtime", severity: "fatal", expected: true,
    },
    WRONG_WORLD: {
        title: "That's on another world",
        description: "A world's calendar reads places on that world. Use the calendar of the world the position is on: `player.world`.",
        source: "runtime", severity: "fatal", expected: true,
    },
    // ── your account and the registry ──────────────────────────────────────
    SIGNED_OUT: {
        title: "You're not signed in",
        description: "Publishing needs your ArcLabs account: run `terra login`.",
        source: "cloud", severity: "fatal", expected: true,
    },
    LOGIN_EXPIRED: {
        title: "The sign-in code expired",
        description: "A code lasts a few minutes. Run `terra login` again and approve it in the browser before it runs out.",
        source: "cloud", severity: "fatal", expected: true,
    },
    ACCOUNT_REFUSED: {
        title: "Your sign-in was refused",
        description: "The key terra holds is no longer accepted: it was revoked or it expired. Run `terra login` again.",
        source: "cloud", severity: "fatal", expected: true,
    },
    REGISTRY_UNREACHABLE: {
        title: "Can't reach the registry",
        description: "The request didn't get an answer. Check the connection; ARCLABS_URL says which server terra talks to.",
        source: "cloud", severity: "fatal", expected: true,
    },
    REGISTRY_REFUSED: {
        title: "The registry refused it",
        description: "The registry checked the request and said no. Its reason is below.",
        source: "cloud", severity: "fatal", expected: true,
    },
    MOD_NOT_PUBLISHED: {
        title: "No such mod on the registry",
        description: "Nothing is published under that name, or the version asked for doesn't exist. Names are @scope/name.",
        source: "cloud", severity: "fatal", expected: true,
    },
    MOD_TOO_NEW: {
        title: "That mod needs a newer terra",
        description: "It was built against a newer mod API than this terra has. Update Terraverse, or install an older version of the mod.",
        source: "cloud", severity: "fatal", expected: true,
    },
    PUBLISH_INVALID: {
        title: "This can't be published as it is",
        description: "Something about the project stops it going to the registry. The detail says what to change.",
        source: "manifest", severity: "fatal", expected: true,
    },
} as const satisfies Record<string, Entry>

type Entry = Pick<AxonErrorLike, "title" | "description" | "source" | "severity"> & { expected?: true }

export type ErrorCode = keyof typeof MAP

export type ErrOpts = {
    /** The specific detail behind the generic title: which mod, which file, which name. */
    detail?: string
    context?: Record<string, unknown>
    cause?: unknown
}

/** A failure with a code from the map. Renders through arcline's errorReport as it is. */
export class TerraError extends Error implements AxonErrorLike {
    readonly code: ErrorCode
    readonly title: string
    readonly description: string
    readonly source: AxonErrorLike["source"]
    readonly severity: AxonErrorLike["severity"]
    readonly expected: boolean
    readonly context: Record<string, unknown> | undefined
    readonly frames: AxonErrorLike["frames"]

    constructor(code: ErrorCode, opts: ErrOpts = {}) {
        const entry: Entry = MAP[code]
        super(opts.detail ?? entry.title, { cause: opts.cause })
        this.name = code
        this.code = code
        this.title = entry.title
        this.description = entry.description
        this.source = entry.source
        this.severity = entry.severity
        this.expected = entry.expected === true
        this.context = opts.context
        // wrapping a thrown error, where it was thrown is what matters, not where it was wrapped
        const stack = opts.cause instanceof Error ? opts.cause.stack : this.stack
        this.frames = stack ? parseStack(stack) : []
    }
}

/** Throw this: `throw err("PREFIX_TAKEN", { detail: "…" })`. */
export function err(code: ErrorCode, opts?: ErrOpts): TerraError {
    return new TerraError(code, opts)
}

/** Anything thrown, as a TerraError: one of ours as it is, anything else as UNEXPECTED. */
export function errOf(thrown: unknown): TerraError {
    if (thrown instanceof TerraError) return thrown
    const detail = thrown instanceof Error ? thrown.message : String(thrown)
    return err("UNEXPECTED", { detail: detail, cause: thrown })
}
