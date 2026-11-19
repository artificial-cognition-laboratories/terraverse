import type { CompanionWire } from "../system/stars"
import type { OverlayId, PreviewWire } from "../worldgen/fields"
import type { WeatherWire } from "../worldgen/weather"
import type { StarId } from "../worldgen/config"

// The contract between the engine (src/shell/) and the UI.
// Change both sides together: intents are answered by verbs registered with
// Shell::on() (shell.cpp, menu.cpp), messages are sent by Shell::send().

/** entering: a world chosen and on its way (src/shell/entering.h); the menu stays up, locked, till its first frame. */
/** leaving: the game is over, its world being saved and closed (nothing in it answers) */
export type Mode = "menu" | "entering" | "game" | "leaving"

/** In-game screens the engine can open (src/shell/screens.cpp: SCREENS). */
export type ScreenName = "pause" | "inventory" | "wiki" | "map"

/** A slot in the local player's inventory, or with `at` a node's (a station's worktop). */
export type NodePos = { x: number; y: number; z: number }
export type SlotRef = { list: string; index: number; at?: NodePos }

/**
 * This build, and the latest signed release: null if it couldn't be asked, or on a development build.
 * `launcher`: started by the launcher, which is where a newer version is chosen.
 */
export type UpdateCheck = { current: string; latest: string | null; available: boolean; launcher: boolean }

export type World = {
    name: string; path: string; code: string | null
    mode: "survival" | "creative" | null; damage: boolean | null
    pinned: boolean; lastOpened: number | null; rules: Record<string, string>
}

/** Local content: reported by src/shell/community.cpp, not a remote catalog. */
export type CommunityTexture = {
    id: string; name: string; description: string; author: string; active: boolean
}
export type CommunityWorld = { name: string; path: string; code: string | null }
export type PublicServer = {
    id: string; name: string; description: string; host: string
    region: string; players: number; capacity: number; pingMs: number | null
    version: string; tags: string[]; mods: string[]; featured: boolean
    favorite: boolean; compatible: boolean
}
export type ServerDirectory = { status: "ready" | "unavailable"; servers: PublicServer[] }
export type CommunityCatalog = {
    textures: CommunityTexture[]; worlds: CommunityWorld[]
}

/** How to show an item; icon is null when it has nothing to draw. */
/** `mass`: kg each, as the game weighs it; null for what it doesn't weigh. */
export type ItemInfo = { description: string; icon: string | null; mass: number | null }
export type CreativeItem = ItemInfo & { item: string; stackMax: number; category: "building" | "nature" | "functional" | "tools" | "items" }

export type Stack = { item: string; count: number; wear: number }

/** A tool in hand, as sculpt::KINDS names it, in its order. */
export const TOOL_IDS = ["box", "cylinder", "cone", "sphere", "cut", "bore", "frame", "truss", "beam"] as const
export type ToolId = (typeof TOOL_IDS)[number]
/**
 * What the right-hand rail asks of the game (src/client/hand.h: Hand::ACTIONS); `save`, `place`, `take` and `forget`
 * take a part's name in the library. `copy`, `paste` and `name` (Ctrl+C, V, S while building) are the keys' (sprint 74).
 */
export const ACTION_IDS = ["focus", "mirror", "undo", "start", "save", "place", "hollow", "level", "reset", "detach", "delete",
    "copy", "paste", "name", "take", "forget", "round"] as const
export type ActionId = (typeof ACTION_IDS)[number]
/** The actions that take a name. */
export const NAMED_ACTIONS = ["save", "place", "take", "forget"] as const satisfies readonly ActionId[]
export type NamedActionId = (typeof NAMED_ACTIONS)[number]
/** What the brush does with a shape (src/client/hand.h: Hand::BRUSHES). Joints are set from a part's menu, in the world. */
export const BRUSH_IDS = ["add", "cut", "paint", "pull", "part"] as const
export type BrushId = (typeof BRUSH_IDS)[number]
/** The brushes the rail picks, in its order (R steps through them). */
export const RAIL_BRUSHES = ["add", "cut", "paint"] as const satisfies readonly BrushId[]
/** piece::JOINTS (src/piece/machine.cpp): wheels steer and are driven by their joint (sprint 73). */
export type JointId = "fixed" | "rotary" | "hinge" | "steers" | "driven" | "steers_driven" | "sprung"
export type Ask = { action: Exclude<ActionId, NamedActionId> } | { action: NamedActionId; name: string }
    | { action: "relink"; other: string; joint: JointId | "none" }
    | { action: "socket"; joint: JointId } | { action: "unsocket"; socket: string }

/** The game as the HUD sees it (src/shell/state.cpp). Each section arrives whole when it changes. */
export type StateSections = {
    hud: { visible: boolean; crosshair: boolean; hotbar: boolean; health: boolean; breath: boolean }
    player: { hp: number; hpMax: number; breath: number; breathMax: number }
    mapPose: { u: number; v: number; heading: number; height: number }
    hotbar: { size: number; selected: number }
    /** The tools in hand (src/sculpt): which and how big (sixteenths), while they're out. */
    tools: { out: false } | {
        /** `focused`: an object's on the workbench. `cursor`: the cursor's free (Tab, or a menu open): the page is clicked. */
        out: true; tool: ToolId; size: number; mirrored: boolean; focused: boolean; cursor: boolean
        /** A part's to be saved (Ctrl+S): the page asks its name (sprint 74). */
        naming: boolean
        /** The brush as it acts: creating, adding makes parts. */
        brush: BrushId
        /** Build mode (sprint 70): what's added is an object (true), or the world's (Alt held). */
        creating: boolean
        /** A new part's joint to what it's drawn on. */
        partJoint: JointId
    }
    /** The player's parts library by name (src/workshop/library.h: every world's), sorted. */
    library: string[]
    inventory: Record<string, { width: number; stacks: (Stack | null)[] }>
}

export type StateMessage = {
    [S in keyof StateSections]: { kind: "state"; section: S; value: StateSections[S] }
}[keyof StateSections]

/** A system's body as the engine holds it, in the page's units (src/shell/system.h: systemJson). */
export type SystemBodyWire = {
    id: string
    name: string
    parent: string | null // null orbits the star; else a planet
    color: string         // its swatch in lists
    phase: number         // 0..1: where it starts on its orbit, for the system view
    /** The playable world: its land, tilt and year are the world code (its Surface, in the World step). */
    playable: boolean
    orbit: number         // AU around the star; parent radii around a planet
    eccentricity: number
    inclination: number   // degrees
    mass: number          // log₁₀ Earth masses (the slider is logarithmic)
    formed: number        // 0..1: hot and close (0) to beyond the frost line (1)
    volatiles: number     // 0..1: the water and gases it formed with
    day: number           // rotation period, hours
    tilt: number          // degrees (the playable world's is in the world code)
    life: boolean         // whether life arises where it can
    spin: SpinWire        // a lock sets the day; `day` is only a free body's
    /** Its own land settings; null: the usual. The start body's are the world code's own. */
    land: BodyLandWire | null
}
/** How a body turns. Must match src/terragen/body.h Spin, in order. */
export const SPIN_WIRES = ["free", "partner", "star", "3:2", "2:1", "5:2"] as const
export type SpinWire = (typeof SPIN_WIRES)[number]
export type BodyLandWire = { continents: number; land: number; coast: number; mountains: number; roughness: number; temperature: number; humidity: number; rivers: number; caves: number; ores: number }
/** A system: its star (StarId), the star's mass (Suns) and metallicity ([Fe/H]), age (billions of years), wind (0..1), bodies (a planet before its moons). */
export type SystemWire = { star: string; starMass: number; metallicity: number; age: number; activity: number; companion: CompanionWire; bodies: SystemBodyWire[] }

/** One body's causes, as `system.preview` takes them (src/shell/system.cpp → terragen/body.h). */
export type BodyCausesWire = {
    id: string
    parent: string | null // null orbits the star; else a planet listed before it
    orbit: number         // AU around the star; parent radii around a planet
    eccentricity: number
    mass: number          // Earth masses
    formed: number        // 0..1: hot and close to beyond the frost line
    volatiles: number     // 0..1: the water and gases it formed with
    day: number           // hours
    life: boolean
    spin: SpinWire
}

export type BodyKind = "temperate" | "cold desert" | "hothouse" | "volcanic" | "ice shell" | "hydrocarbon" | "airless" | "giant"

/** What a body is today, and how it got there — everything the page shows about it comes from here. */
export type BodyCard = {
    id: string
    kind: BodyKind
    giant: boolean
    metal: number; rock: number; ice: number
    radius: number; gravity: number   // Earth = 1
    escape: number                    // km/s
    sunlight: number                  // Earth = 1
    temperature: number               // kelvin (cloud tops for a giant)
    equilibrium: number               // kelvin, without its air
    pressure: number | null           // bar; null: a giant, no surface
    gases: Partial<Record<"CO2" | "N2" | "O2" | "CH4" | "H2" | "He", number>>
    field: number; heat: number; tidal: number
    ocean: boolean; plates: boolean; volcanism: boolean; locked: boolean; spin: SpinWire; day: number /* hours a turn takes, as it spins */
    /** Its shells, centre outward: outer edge as a share of the radius. */
    layers: { name: "core" | "mantle" | "crust" | "ocean" | "ice" | "metallic hydrogen" | "envelope"; outer: number; molten: boolean }[]
    history: { age: number; text: string }[]  // billions of years after formation
}

/** An element's identity, for the periodic table (matter/elements.json). Grid row 9–10: the f-block. */
export type MatterElement = {
    number: number; symbol: string; name: string; mass: number
    category: "nonmetal" | "noble" | "alkali" | "alkaline" | "metalloid" | "halogen" | "post" | "transition" | "lanthanide" | "actinide"
    period: number; group: number | null; row: number; col: number
    color: string   // for drawing its atoms
    radius: number  // covalent radius, pm
}

export type MatterStructure =
    | { type: "molecule"; atoms: [string, number, number, number][]; bonds: [number, number, number][] } // ångströms; bond order
    | { type: "crystal"; lattice: "fcc" | "bcc" | "diamond" | "rocksalt" | "layered"; atoms: string[] }

/** A substance with its phase anchors (matter/substances.json), its phase boundaries sampled by the engine (src/matter). */
export type MatterSubstance = {
    id: string; name: string; formula: string
    kind: "element" | "compound"
    composition: Record<string, number>
    molar_mass: number
    triple: [number, number]; critical: [number, number] // [kelvin, pascals]
    boil: number | null; sublime?: number                // at 1 atm
    fusion: number; vaporization: number                 // kJ/mol
    density: { solid: number; liquid: number }           // kg/m³
    conductivity: { solid: number; liquid: number; gas: number } // W/(m·K)
    heat_capacity: number                                // J/(kg·K)
    hardness?: number                                    // Mohs
    approximate: boolean
    note: string
    structure: MatterStructure
    curves: { sublimation: [number, number][]; vapour: [number, number][]; melting: [number, number][] }
}

export type MatterState = "solid" | "liquid" | "gas" | "supercritical"

/** A reaction (matter/reactions.json): balanced on load by the engine. */
export type MatterReaction = {
    id: string; name: string; note: string
    reactants: Record<string, number>; products: Record<string, number>
    heat: number        // kJ released per reaction as written (negative: takes heat)
    activation: number  // kJ/mol
    factor: number      // per second
    energy?: "electricity" | "light"
    requires?: { substance: string; state: MatterState }[]
}

/** A rock: a named mixture of minerals by mass (matter/rocks.json). */
export type MatterRock = {
    id: string; name: string; note: string
    origin: "igneous" | "metamorphic" | "sedimentary" | "sediment" | "impact"
    minerals: Record<string, number>
}

/** A mixture as the Reactor holds it between steps; the engine steps it (`matter.react`). */
export type MatterMixture = {
    amounts: Record<string, number> // moles
    temperature: number             // kelvin
}

/** One block of a sandbox, as the page sets it (src/shell/wiki.cpp): solids or liquids by share of its volume, or a gas by mole share. */
export type SandboxBlock = {
    at: [number, number, number] // y is up
    node?: string                // a block of the game's, by name
    add?: Record<string, number> // moles of substances put in it (a crucible's charge)
    temperature?: number         // K
    power?: number               // watts put into it (what's heating it, if it can run short)
    hold?: boolean               // kept at that temperature (what's heating it)
}

/** A few blocks of the world to make (src/sandbox/stage.h): blocks along each side (1..16), the room around it. */
export type SandboxSpec = {
    size: [number, number, number]
    ambient: number  // K
    pressure: number // Pa
    air: Record<string, number> // the room's gas by mole share
    blocks: SandboxBlock[]
    blows?: { at: [number, number, number]; rate: number; until: number }[] // the room's air pushed in: moles a second, till then (s)
    pours?: { at: [number, number, number]; substance: string; rate: number; temperature?: number; until: number }[] // moles a second, till then (s)
}

/** One of the wiki's Movement scenes (game/moves.json). */
export type SceneKind = "movement" | "vehicles" | "ground"
export type MoveScene = { id: string; group: string; title: string; keys: string; note: string; later?: boolean }
export type MoveState = { time: number; doing: string; pace: number; mass?: number }

export type ReliefState = { round: number; rounds: number }

/** A place a plant grows, as terragen knows a column (src/flora/flora.h: Site). */
export type FloraSite = {
    heat: number; wet: number; season: number; fertility: number; sand: number; clay: number; stones: number
    depth: number; wind: number; slope: number; upset: number; light: number; bank: boolean
}
export type FloraLineage = { name: string; preset: string; group: string; leaf: string; bark: string; lives: boolean }
/** One of a plant's genes (src/flora/flora.h: GENES): what it was born with or set to, and what its site grew of it. */
export type FloraGene = { id: string; label: string; group: string; lo: number; hi: number; whole: boolean; value: number; grown: number; set: boolean }
/** What grew there (src/shell/flora.cpp); `limit` and `why`: none, cold, drought, hunger, rooting, shade or wet. */
export type FloraState = {
    lineages: FloraLineage[]
    genes: FloraGene[]
    strategy: { warmth: number; water: number; light: number; food: number; rooting: number; vigour: number; disturbance: number; strategy: string; limit: string }
    traits: { lives: boolean; why: string; evergreen: boolean; leafing: number; flowering: number; leaf: string }
    /** Whole blocks' worth of each part. */
    blocks: Record<string, number>
    /** Its voxels to a block's side. */
    grain: number
}

export type SceneRect = { x: number; y: number; width: number; height: number }

export type SandboxFrame = {
    size: [number, number, number]
    time: number          // s simulated
    main: number[]        // index into matter.list's substances, −1: only gas
    state: MatterState[]  // of `main`
    fill: number[]        // share of the block that's solid or liquid
    temperature: number[] // K
    activity: number[]    // W of reaction heat (+ released)
    vapour: number[]      // the gas most unlike the room's air, −1: none
    haze: number[]        // 0..1: how much of the block's gas that is
}


/**
 * Values game Lua sets for this player's page (player:set_ui). The page keeps
 * the latest per key; null clears it. Each key's owner is noted.
 */
export type UiValues = {
	/** The engine (server/player.cpp): the server permits this player to use the creative catalogue. */
	creative: boolean
    /**
     * The engine (server/player.cpp): the body the player's on and what they've explored of it.
     * `code` is the world's; `body` (an atlas world's) names which of its
     * bodies, whose world is its own (size, radius); `bodies` every body
     * that can be stood on, the home first.
     */
    map: { code: string; body?: string; name?: string; bodies?: { id: string; name: string }[]; size: number; radius: number; sea: number; star: StarId; tilt: number; mode: "creative" | "survival"; explored: string; grid: number }
    /** The engine (server/player.cpp): the player's place on the world model and the current date. */
    map_position: { x: number; z: number; body?: string; date: number }
    /** The engine (server/player.cpp): the world's rules give this player the wiki (key G) */
    wiki: boolean
    /** The engine (server/matter.cpp): the reactions this player has seen happen in the world (ids) */
    discovered: string[]
    /** The engine (server/player.cpp): the body the player is on (an atlas world's; `home` the world's own) */
    body: { id: string; name: string; home: boolean }
    /** The engine (server/planet.cpp): travelling to another body (/travel): the screen while its land is made */
    travel: { body: string; name: string }
    /** The engine (server/weather.cpp): the weather where the player stands */
    weather: {
        temperature: number // °C
        humidity: number    // 0..1
        precipitation: number // mm/h
        snow: boolean
        wind: { x: number; z: number } // m/s, east and north
        pressure: number    // hPa
        events?: string[]   // thunderstorm, blizzard, fog, hurricane, drought, ashfall
    }
}


export type UiMessage = {
    [K in keyof UiValues]: { kind: "ui"; key: K; value: UiValues[K] | null }
}[keyof UiValues]

/** Events the page sends game Lua (core.register_on_ui_event), by name. */
export type UiEvents = {
	/** Creative: go to the ground at a place (radians); the engine (server/player.cpp) checks and moves the player. */
	"map.teleport": { latitude: number; longitude: number }
	/** Replace one hotbar slot from the catalogue; the engine (server/player.cpp) validates it. */
	"creative.take": { item: string; slot: number; count: number }
	"creative.clear": { slot: number }
}

export type UiEvent = { [E in keyof UiEvents]: { name: E; payload: UiEvents[E] } }[keyof UiEvents]

/**
 * The player's ArcLabs account (src/shell/account.cpp). Signing in is the
 * device flow: `pending` holds the code the browser's approval page shows.
 * Sent whole as an `account` message whenever it changes.
 */
export type Account =
    | { status: "signedOut"; problem?: string }
    | { status: "starting" }
    | { status: "pending"; code: string; url: string }
    | { status: "checking" }
    | { status: "signedIn"; id: string; username: string; name: string | null }
    | { status: "unreachable"; problem: string }

/** An ArcLabs player, as Terraverse's friends list shows them. */
export type Player = { id: string; username: string; name: string | null }
/** Where a player is: the menu, someone's world, or their own (hosting; `open`: friends can join). */
export type Presence = { state: "menu" | "playing" | "hosting"; world: string | null; open: boolean }
/** A friend, with where they are now (null: offline). */
export type Friend = Player & { presence: Presence | null }
export type Invite = { id: string; from: Player; world: string; expiresAt: string }

/**
 * Friends, presence and invites (src/shell/social.cpp ← arclabs
 * platform/terrafirma/social.ts). Null while signed out. `view` is null until
 * the backend first answers; `problem` says why it may be stale.
 */
export type Social = {
    me: Presence
    /** In the player's own world: they can open it to friends and invite. */
    joinable: boolean
    view: { friends: Friend[]; incoming: Player[]; outgoing: Player[]; invites: Invite[] } | null
    problem?: string
}

/**
 * The Planetarium (src/shell/planetarium.h): the native planet drawn under the
 * page while shown. Places are radians; a cell is the atlas's
 * (terragen::Atlas::Cell) at the sea there.
 */
export type PlanetariumPlace = { latitude: number; longitude: number }
export type PlanetariumCell = { region: "segment" | "cap" | "core"; index: number; i: number; j: number; k: number }
/**
 * What's over a body's land (planetarium.layer): an overlay painted natively from the world
 * model as each tile is built (as sharp as the land; on every body shown, unless `explored`
 * masks it to what the player's seen of theirs: server/player.cpp's grid, a bit a cell, base64), on a
 * date (0..1 through the year; none: the year's mean); an image the page painted (the weather:
 * RGBA, base64, the preview's layout); the body cut open; or nothing.
 */
export type PlanetariumLayer =
    | { overlay: Exclude<OverlayId, "terrain" | "weather">; date?: number; explored?: { grid: number; mask: string } }
    | { width: number; height: number; rgba: string }
    | { inside: true }
    | { none: true }

/** The composed views (src/shell/planetarium.h): the main menu's, and Play's (the selected world in full view, mostly lit). */
export type PlanetariumShot = "home" | "play"

/** Where `planetarium.fly` takes the camera: what's left out stays as it is. */
export type PlanetariumFlight = {
    body?: string // a body's id, or "star"
    latitude?: number
    longitude?: number
    altitude?: number // blocks over the sea; or
    radii?: number    // the body's radii over its sea
    lit?: boolean     // over the side the star lights (no latitude or longitude with it)
    seconds: number
}

export type PlanetariumMarker = PlanetariumPlace & { colour: string /* "#rrggbb" */; altitude?: number /* blocks over the sea */ }
/** In game the body is always the world being played (null); on the create screen, the world being made. */
/** A world (its code); in game, `body` one of its bodies (by id), drawn from its tiles. */
export type PlanetariumBody = null | { code: string; body?: string }
/** Where the planet is drawn: a part of the page (page pixels), or the whole window. (A request is spread into the message, so it's never null.) */
export type PlanetariumViewport = { whole: true } | { x: number; y: number; width: number; height: number }
/** A body under the mouse: who it is, and its disc on screen (page pixels). */
export type PlanetariumHover = { id: string; name: string; kind: string; x: number; y: number; radius: number }
/** One stage of making a world: kept (read from what was kept), so far in seconds; `share` 0..1 while running, if it counts. */
export type LoadingStage = { id: string; label: string; state: "waiting" | "running" | "done" | "kept"; seconds: number; share: number | null }
/** A world being made: how far as a whole (0..1, weighed by each stage's usual time), and its stages in order. */
export type Loading = { share: number; stages: LoadingStage[] }
export type PlanetariumMessage =
    | { kind: "planetarium"; event: "picked"; latitude: number; longitude: number; cell: PlanetariumCell }
    /** The block under the mouse in a scene (index into its frame), −1 for none. */
    | { kind: "scene"; event: "hover"; index: number }
    /** The host's clock while it runs (the menu's): the date, 0..1 through the year. */
    | { kind: "planetarium"; event: "time"; date: number; running: boolean }
    /** What's under the mouse, as it changes or moves: where it's drawn (page pixels); the star's id "star". */
    | { kind: "planetarium"; event: "hovered"; body: PlanetariumHover | null }
    /** A double click flew the camera to a body (the star: out to the whole system). */
    | { kind: "planetarium"; event: "chosen"; body: string }
    /** A body asked for is being made: how far, by stage (shell/loading.h); null once it's shown. */
    | { kind: "planetarium"; event: "making"; loading: Loading | null }

/** Engine → UI */
/** Where the debugger sits beside the game. */
export type DockSide = "right" | "bottom"
export type LogLevel = "error" | "warning" | "action" | "info" | "verbose"
/** One line of the engine's log. `seq` counts from the engine's start; `time` is ms since the epoch. */
export type LogLine = { seq: number; time: number; level: LogLevel; thread: string; text: string }
/**
 * One frame, from the recorder (trace/trace.h). Times are ms since the engine
 * started. `threads` is each thread's time in each zone it closed during the
 * frame, by name, nested ones included (a zone's time is in its parent's too).
 * `counters` is each value reported during the frame (blocks drawn, draw
 * calls, packets: the old profiler's pages), folded as its kind says.
 */
export type PerfFrame = {
    id: number; start: number; end: number
    threads: Record<string, Record<string, number>>
    counters: Record<string, number>
    /** What the client sent and received, flat, five numbers a type: type, 0 in / 1 out, count, bytes, largest. */
    net: number[]
}
/** The client's messages over a range: `messages` flat, six numbers each (time ms, 0 in / 1 out, type, channel, reliable, bytes); `heads` each incoming one's first payload bytes, as hex. */
export type NetCapture = { messages: number[]; heads: string[] }
/** A player on the local server, as the Server tab lists them. `rtt` ms, null unknown; `connected` seconds. */
export type ServerPlayer = { name: string; position: [number, number, number]; hp: number; rtt: number | null; connected: number; protocol: number }
/** A mod the local server runs: `source` the game's own or one the player added. */
/** A TypeScript mod the local server runs (its server folder's .terra/manifest.json). */
export type ServerMod = { name: string; version: string; source: "game" | "user" }
/** The server the game's connected to. Local: all of it; remote: `local` false and its address alone. */
export type ServerSummary =
    | { local: true; address: string; version: string; game: string; world: string; uptime: number; lag: number; players: ServerPlayer[]; mods: ServerMod[] }
    | { local: false; address: string }
/** The game process: CPU as a share of one core (%), resident memory (bytes), and each thread where the OS says. */
export type ProcessSample = { pid: number; cpu: number; rss: number; threads: { name: string; cpu: number }[] }
/** A span kept for a waterfall: `lane` "startup" for the server's start (its init, each mod's load). ms since the engine started. */
export type TraceMark = { lane: string; what: string; start: number; end: number }
/** A moment on the connection; `command` set when it's the first arrival of that message type since connecting. */
export type NetEvent = { time: number; what: string; command: number }
/**
 * Every thread's zones over a range, whole. `spans` is flat, four numbers a
 * zone: name (an index into names), depth, start ms, end ms. `since` is the
 * oldest zone the thread still had; before it, its ring had moved on. Every
 * thread is listed, idle ones too.
 */
export type PerfCapture = { names: string[]; tracks: { thread: string; since: number; spans: number[] }[] }

export type EngineMessage =
    | { kind: "mode"; mode: Mode }
    /** The debugger's page only: the side it's docked on, sent when ready and on every change. */
    | { kind: "debugger.dock"; side: DockSide }
    /** The debugger's page only: what the engine has logged since it last sent (all it kept, to a fresh page). */
    | { kind: "debugger.log"; lines: LogLine[] }
    /** The debugger's page only: frames ended since the last, ~10 times a second (all kept, to a fresh page). */
    | { kind: "perf.frames"; frames: PerfFrame[]; units: Record<string, string> }
    /** The debugger's page only: message types' names by number, each way, once per page. */
    | { kind: "net.names"; names: { in: string[]; out: string[] } }
    /** The debugger's page only: connection events as they happen (all kept, to a fresh page). */
    | { kind: "net.events"; events: NetEvent[] }
    /** The debugger's page only, once a second: the server (null outside a world) and the process. */
    | { kind: "server.summary"; server: ServerSummary | null; process: ProcessSample }
    /** The debugger's page only: marks made since the last (all kept, to a fresh page). */
    | { kind: "server.marks"; marks: TraceMark[] }
    /** How far the world being entered has got, by stage; null once it's there (or it never got there). */
    | { kind: "entering"; loading: Loading | null }
    | { kind: "error"; message: string }
    | { kind: "items"; items: Record<string, ItemInfo> }
    | { kind: "screen"; screen: ScreenName | null }
    | { kind: "account"; account: Account }
    | { kind: "social"; social: Social | null }
    | PlanetariumMessage
    | StateMessage
    | UiMessage

/** UI → Engine: each intent's request and the engine's response. */
export type Intents = {
    ready: { request: {}; response: null }
    capture: { request: { keyboard: boolean }; response: null }
    /** The debugger's page only (debugger.html), from here to debugger.close. */
    "debugger.dock": { request: { side: DockSide }; response: null }
    /** Its edge is held: the engine sizes it by the mouse until the button comes up. */
    "debugger.drag": { request: {}; response: null }
    "debugger.close": { request: {}; response: null }
    /** Every thread's zones over [from, to] ms, whole: what the recorder still has of them. Answered off the game's thread. */
    "perf.capture": { request: { from: number; to: number }; response: PerfCapture }
    /** Everything the recorder has, written as a Chrome trace (Perfetto, chrome://tracing). */
    "perf.export": { request: {}; response: { path: string } }
    /** The client's messages over [from, to] ms, whole. Answered off the game's thread. */
    "net.capture": { request: { from: number; to: number }; response: NetCapture }
    "update.check": { request: {}; response: UpdateCheck }
    "worlds.list": { request: {}; response: World[] }
    "worlds.pin": { request: { path: string; pinned: boolean }; response: null }
    "worlds.rename": { request: { path: string; name: string }; response: null }
    "servers.list": { request: {}; response: ServerDirectory }
    "servers.join": { request: { id: string }; response: null }
    "servers.favorite.set": { request: { id: string; favorite: boolean }; response: null }
    "community.list": { request: {}; response: CommunityCatalog }
    /** User-installed mods are selected per world; built-ins are required by this build. */
    /** A local texture pack is used after restarting Terraverse; empty id restores defaults. */
    "community.texture.set": { request: { id: string }; response: null }
    /** settings: written to the world's world.mt — creative_mode, enable_damage, tf_* (src/shell/worlds.cpp) */
    "worlds.create": { request: { name: string; code: string; settings: Record<string, string | number | boolean> }; response: World }
    /** terragen's preview of a world code; date 0..1 through the year (src/shell/menu.cpp) */
    /** `body`: in game, a body of the world (its world is its own, not a code) */
    "worldgen.preview": { request: { code: string; body?: string; date: number | null; resolution: number }; response: PreviewWire }
    /** a settled year of the world's weather, `snapshots` maps through it (src/shell/menu.cpp) */
    "weather.preview": { request: { code: string; snapshots: number }; response: WeatherWire }
    "worlds.delete": { request: { path: string }; response: null }
    /** The building blocks: every element, and every substance with its phase curves (src/shell/matter.cpp) */
    "matter.list": { request: {}; response: { elements: MatterElement[]; substances: MatterSubstance[]; reactions: MatterReaction[]; rocks: MatterRock[] } }
    /** The wiki's sandboxes: tiny pieces of the universe run with the game's physics (src/sandbox) */
    "sandbox.create": { request: SandboxSpec; response: { id: number } }
    "sandbox.step": { request: { id: number; seconds: number }; response: SandboxFrame }
    "sandbox.drop": { request: { id: number }; response: null }
    /** A sandbox drawn natively where the page leaves room (src/shell/scene.h); */
    "moves.list": { request: { scenes: SceneKind }; response: MoveScene[] }
    "moves.show": { request: { scenes: SceneKind; id: string; rect: SceneRect; right: number }; response: null }
    "moves.set": { request: { speed: number; running: boolean }; response: null }
    "moves.restart": { request: {}; response: null }
    "moves.state": { request: {}; response: MoveState }
    "relief.show": { request: { rect: SceneRect; right: number }; response: null }
    "relief.set": { request: { lens: string; speed: number; running: boolean }; response: null }
    "relief.restart": { request: {}; response: null }
    "relief.state": { request: {}; response: ReliefState }
    "flora.show": { request: { rect: SceneRect; right: number }; response: null }
    "flora.set": { request: { world: string; lineage: number; plant: number; life: { age: number; season: number }; genes: Record<string, number>; site: FloraSite }; response: FloraState }
    "scene.show": { request: { id: number; rect: SceneRect; right: number; heat: boolean }; response: null }
    "scene.place": { request: { rect: SceneRect; right: number }; response: null }
    "scene.hide": { request: {}; response: null }
    /** Newton's cannonball on the world's planet (src/shell/wiki.cpp: orbit::Flight); points in planet radii, x east, y up */
    "orbit.coast": {
        request: { altitude: number; speed: number; angle: number; mass: number; drag_area: number; seconds: number }
        response: { radius: number; scale_height: number; seconds: number; end: "ground" | "around" | "time"; bound: boolean
            periapsis: number; apoapsis?: number; orbital_speed: number; points: [number, number][] }
    }
    /** Step a mixture for `seconds` in its surroundings: what it's become, the states of what's in it, and each running reaction's rate (per second) */
    "matter.react": {
        request: MatterMixture & { surroundings: number; pressure: number; power: number; light: number; seconds: number; spark: boolean }
        response: MatterMixture & { states: Record<string, MatterState>; rates: Record<string, number> }
    }
    /** What a substance is at a temperature (K) and pressure (Pa), and where it would boil and melt at that pressure */
    "matter.state": { request: { id: string; t: number; p: number }; response: { state: MatterState; boils_at: number | null; melts_at: number | null } }
    /** terragen's body model: every body's card and history from the system's causes (src/shell/system.cpp) */
    "system.preview": { request: { star: string; starMass: number; metallicity: number; age: number; activity: number; companion: CompanionWire; bodies: BodyCausesWire[] }; response: BodyCard[] }
    /** The system a new world's draft starts from (the menu's). */
    "system.home": { request: {}; response: SystemWire }
    /** A world's own system and every body's card (the game's: its map). */
    "system.world": { request: { code: string }; response: SystemWire & { cards: BodyCard[] } }
    play: { request: { path: string }; response: null }
    /** Every Lab scenario, from the game's lab.json files (src/shell/lab.cpp). */
    /** Builds that scenario's own world and enters it (src/shell/worlds.cpp: Worlds::lab). */
    /** An event for game Lua (src/shell/ui.cpp). */
    "ui.send": { request: UiEvent; response: null }
    quit: { request: {}; response: null }
    "settings.get": { request: {}; response: Record<string, number | boolean | string> }
    "settings.set": { request: { key: string; value: number | boolean | string }; response: null }
    "settings.apply": { request: { values: Record<string, number | boolean | string> }; response: null }
    "settings.bindings.get": { request: {}; response: Record<string, string> }
    "settings.bindings.set": { request: { key: string; scancode: number; replace: boolean }; response: null }
    "settings.bindings.reset": { request: {}; response: null }
    open: { request: { url: string }; response: null }
    /** The ArcLabs account (src/shell/account.cpp); changes arrive as `account` messages. */
    "account.get": { request: {}; response: Account }
    /** Starts the device flow and opens the browser on the approval page. */
    "account.signIn": { request: {}; response: null }
    "account.cancel": { request: {}; response: null }
    /** Forgets the key on this machine. */
    "account.signOut": { request: {}; response: null }
    /** Checks the saved key again after ArcLabs couldn't be reached. */
    "account.retry": { request: {}; response: null }
    /** Friends, presence and invites (src/shell/social.cpp); changes arrive as `social` messages. */
    "social.get": { request: {}; response: Social | null }
    /** Friends can join the world you're in (your own). */
    "social.open": { request: { open: boolean }; response: null }
    /** Ask a player to be friends, by exact ArcLabs username; accepts if they'd asked you. */
    "social.request": { request: { username: string }; response: null }
    "social.accept": { request: { id: string }; response: null }
    /** Unfriend, decline, or take back a request. */
    "social.remove": { request: { id: string }; response: null }
    /** Invite a friend into the world you're in. */
    "social.invite": { request: { id: string }; response: null }
    "social.dismiss": { request: { id: string }; response: null }
    /**
     * Join a friend's world (menu only): asks STUN and ArcLabs, then the menu
     * leaves for it (src/shell/menu.cpp → src/online/join.h). `name` is theirs, for messages.
     */
    "social.join": { request: { id: string; name: string }; response: null }
    /** Opens a screen the page decides on (src/shell/screens.cpp: SCREENS); one that needs a UI value stays shut without it. */
    "screen.open": { request: { name: ScreenName }; response: null }
    /** Terraverse: a tool picked on the rail (src/client/hand.h). */
    "tools.pick": { request: { tool: ToolId }; response: null }
    "tools.ask": { request: Ask; response: null }
    "tools.brush": { request: { brush: BrushId; joint?: JointId }; response: null }
    "hotbar.select": { request: { slot: number }; response: null }
    "screen.close": { request: {}; response: null }
    /** Shows a body under the page (the last one stays on screen until it's ready); markers and focus at once if given. */
    "planetarium.show": { request: { body: PlanetariumBody; focus?: PlanetariumPlace & { altitude?: number }; markers?: PlanetariumMarker[]; viewport?: PlanetariumViewport }; response: null }
    /** Where the page leaves room for the planet (page pixels); null: the whole window. */
    "planetarium.viewport": { request: PlanetariumViewport; response: null }
    "planetarium.hide": { request: {}; response: null }
    /** Puts the camera over a place, at an altitude (blocks over the sea) if given. */
    /** Over a place on the body, or to another body of its system (by id). */
    /** Eases the camera to a body (by id), over a place on it, at an altitude (blocks) or so many of its radii up, over `seconds`. */
    "planetarium.fly": { request: PlanetariumFlight; response: null }
    /** What the page covers round the window (page pixels): the planet is framed in the rest. */
    /** What's over the land: an image (RGBA, base64; the preview's layout: row 0 north, column 0 at −180°), the body the camera's round cut open, or nothing. */
    "planetarium.layer": { request: PlanetariumLayer; response: null }
    /** The host's clock (the menu's): stopped or running, and the date (0..1 through the year). */
    "planetarium.time": { request: { running?: boolean; date?: number }; response: null }
    /** Whether the atlas's seams are drawn. */
    "planetarium.seams": { request: { shown: boolean }; response: null }
    /** Whether the bodies' orbits are drawn, for a page that wants the view clear. */
    "planetarium.orbits": { request: { shown: boolean }; response: null }
    /** A menu's composed view (src/planetarium/shot.h), eased to over `seconds` from wherever it's framed; the mouse doesn't move it. null: the page's own view. */
    "planetarium.scene": { request: { shot: PlanetariumShot | null; seconds: number }; response: null }
    "planetarium.frame": { request: { left?: number; right?: number; top?: number; bottom?: number }; response: null }
    "planetarium.focus": { request: (PlanetariumPlace & { altitude?: number }) | { body: string }; response: null }
    "planetarium.markers": { request: { markers: PlanetariumMarker[] }; response: null }
    "game.leave": { request: {}; response: null }
    "game.quit": { request: {}; response: null }
	"creative.list": { request: {}; response: CreativeItem[] }
    /** No `to.index`: the first slot in the list that fits. */
    "inventory.move": { request: { from: SlotRef; to: { list: string; index?: number; at?: NodePos }; count: number }; response: null }
    /** Craft from the grid into craftresult; `collect` moves the result straight into main. */
    "inventory.craft": { request: { count: number; collect: boolean }; response: null }
    "inventory.drop": { request: { from: SlotRef; count: number }; response: null }
}

export type IntentKind = keyof Intents

/** Failure codes from bridge.cpp. */
export const FailureCode = {
    Malformed: 1,
    UnknownIntent: 2,
    Refused: 3, // an expected "no": show the message to the player
    EngineError: 4,
} as const

/** Injected by the engine's renderer process (CefMessageRouter). */
export type TerraQuery = (query: {
    request: string
    onSuccess: (response: string) => void
    onFailure: (code: number, message: string) => void
}) => number

declare global {
    interface Window {
        terraQuery?: TerraQuery
        __terra?: { receive: (message: EngineMessage) => void }
    }
}
