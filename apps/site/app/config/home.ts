import type { IconId } from "./icons"

/**
 * The landing page as data: every section reads its words and pictures from here.
 *
 * `flags` say which sections show. Some describe things that don't exist yet
 * (hosting, agents, the live registry): they're on while the design settles, and
 * each turns off in one line when it's time to ship only what's real.
 */
export const flags = {
    // off until the maps are worth the section: hypsometric, in the page's palette
    universe: false,
    registry: true,
    servers: true,
    hosting: true,
    agents: true,
}

/** Where the page's buttons go. The registry's web view isn't built yet: its page says so. */
export const links = {
    registry: "/registry",
    browse: "/registry/browse",
    docs: "/docs",
    wiki: "/wiki",
    mods: "/docs/mods",
    server: "/docs/server",
    host: "/docs/cli/serve",
    cli: "/docs/cli",
    install: "/docs/install",
}

export const hero = {
    kicker: "Free and open source · Open beta",
    title: ["A universe to", "play in."],
    lede: "A survival game on real planets, made of matter that melts, burns and flows. Then every mod, world and server the community makes on top.",
    /** The seed the page's sky and ground are drawn from */
    code: "TF24.dGVycmE",
}

export const facts = ["Open source", "Multiplayer", "TypeScript mods", "Agent API"]

/** What it is: the block game you know, and where this one goes past it. */
export const limits = {
    kicker: "What it is",
    title: "The voxel game you know. Without the limits.",
    lede: "Dig, build and survive in a world of blocks, the way you'd expect. Underneath, it's made to be built on, and to go where block games never went.",
    items: [
        { icon: "sky", now: "Whole planets, in real star systems", text: "Walk all the way around one, then fly to its moon. Every world comes from a code you can share." },
        { icon: "matter", now: "Matter that melts, burns and flows", text: "Substances heat, react and change state. Water boils away on the Moon because of what water is." },
        { icon: "machines", now: "Real physics underneath", text: "Parts, joints and engines on a physics engine. Build the car, then drive it over real terrain." },
        { icon: "library", now: "One typed API, the same as ours", text: "TypeScript end to end. Every official mod is built on it, so anything the game does, yours can." },
        { icon: "servers", now: "A server in a minute", text: "Hosted by us, or `terra serve` on any machine. Friends join from their list, no addresses." },
        { icon: "open", now: "Free and open source", text: "Free to play, forever. The engine is LGPL and the whole thing is built in the open." },
    ] satisfies { icon: IconId; now: string; text: string }[],
}

export const universe = {
    kicker: "Every world",
    title: "Every world is a real planet.",
    lede: "Plates raised its mountains, rain carved its rivers, and climate decided what grows. Step through one region of one world, layer by layer, as the generator made it.",
    /** One crop of one world in four layers (scripts/bake.sh) */
    layers: [
        { id: "plates", title: "Plates", text: "Continents drift and collide. Where they meet, ranges rise.", image: "/home/layer-plates.webp" },
        { id: "relief", title: "Terrain", text: "Ranges weather into valleys, and rivers run to the sea.", image: "/home/layer-relief.webp" },
        { id: "rain", title: "Rain", text: "Winds carry the sea's water inland until the mountains wring it out.", image: "/home/layer-rain.webp" },
        { id: "biomes", title: "Climate", text: "Heat and rain decide what grows, from ice cap to jungle.", image: "/home/layer-biomes.webp" },
    ],
}

export const registry = {
    kicker: "The registry",
    title: "Made by everyone.",
    lede: "Mods, whole games, worlds, shaders and texture packs. One click to add them to a world, one command to publish your own.",
}

export interface Plan {
    name: string
    /** Who it's sized for, in a few words */
    for: string
    specs: string[]
    /** null until pricing is set: the card says so */
    price: string | null
    featured: boolean
}

export const hosting = {
    kicker: "Hosting",
    title: "Your server, running in a minute.",
    lede: "Pick a world, add the mods, invite your friends. We run it, back it up and keep it updated, and it's the same server folder you could run yourself.",
    steps: [
        { title: "Pick a world", text: "Any world code, or one from the registry." },
        { title: "Add the mods", text: "A whole game, or a few rules of your own." },
        { title: "Invite people", text: "Friends join from their list. No addresses." },
    ],
    // TEMP(2026-11-21): sample plans while the design settles (`flags.hosting`);
    // sizes and prices are set before launch.
    plans: [
        { name: "Friends", for: "A world for you and a few others", specs: ["Up to 10 players", "1 world", "Any mods", "Daily backups"], price: null, featured: false },
        { name: "Community", for: "A server people find and come back to", specs: ["Up to 50 players", "3 worlds", "Any mods", "Hourly backups", "Listed in Live servers"], price: null, featured: true },
        { name: "Network", for: "Several servers, one community", specs: ["100+ players", "Unlimited worlds", "Your own mods, private", "Hourly backups", "Choose your region"], price: null, featured: false },
    ] satisfies Plan[],
    self: {
        title: "Or host it yourself. Free, forever.",
        text: "Any machine, no window. The same folder, the same mods; move it to us or away whenever you like.",
        command: "terra serve",
    },
}

export interface CodeTab {
    label: string
    file: string
    /** A Shiki language */
    lang: "ts" | "console"
    code: string
}

export const build = {
    kicker: "Build anything",
    title: "Say what things are. The universe works out the rest.",
    lede: "Mods are TypeScript, written against the same public API the official game uses. Switch every official mod off and you have a clean universe to start a different game in.",
    points: [
        { title: "Typed end to end", text: "Every block, item and substance is in your editor." },
        { title: "One CLI", text: "Make, run, reload and publish from the folder you're in." },
        { title: "Same API as ours", text: "If the game can do it, your mod can." },
    ],
    // The CLI tab is `terra mod init`'s real output; the others are what it scaffolds, grown a little
    tabs: [
        {
            label: "Mod",
            file: "kilns/server/main.ts",
            lang: "ts",
            code: `content.blocks.create("kilns:granite_brick", {
    matter: "granite",
    texture: "granite_brick.png",
})

game.players.on("join", player => {
    player.tell(\`Welcome, \${player.name}.\`)
})

game.commands.create(["kiln", "light"], {
    description: "Light the kiln you're looking at",
    run({ player, reply }) {
        light(player.world, kilnInView(player))
        reply("Lit.")
    },
})`,
        },
        {
            label: "Server",
            file: "longhaul/server.config.ts",
            lang: "ts",
            code: `export default defineServer({
    title: "Longhaul",

    // The star system: every planet in it is a world here
    system: "TF24.dGVycmE",
    worlds: { verdant: "@verdant", ember: "@ember" },

    settings: {
        "@terraverse/survival": { hunger: "gentle" },
    },
    access: { join: "friends" },
})`,
        },
        {
            label: "CLI",
            file: "terminal",
            lang: "console",
            code: `$ terra mod init kilns
Creating mod kilns

✓ Scaffolding  2ms
✓ Installing   4ms
✓ Generating   3ms

./kilns
  ├─ server
  │   └─ main.ts
  ├─ mod.config.ts
  └─ package.json

$ cd kilns && terra dev`,
        },
    ] satisfies CodeTab[],
}

export const agents = {
    kicker: "For agents",
    title: "Agents play too.",
    lede: "A first-class API for headless control. An agent joins as a player, sees what a player sees and acts through the same verbs, so it can explore, build, test your mod or run a world for you.",
    points: [
        { title: "Headless", text: "No window, no GPU. A world in a process." },
        { title: "Same verbs as players", text: "Move, look, dig, place, craft, chat." },
        { title: "Scriptable worlds", text: "Spin up, drive, snapshot and replay." },
    ],
    transcript: [
        { who: "agent", text: "join TF24.dGVycmE as @scout" },
        { who: "world", text: "spawned · temperate coast · 14:20 local" },
        { who: "agent", text: "look for: copper ore within 200 m" },
        { who: "world", text: "malachite in a granite contact · 82 m NE · 12 m down" },
        { who: "agent", text: "walk to it, dig down" },
        { who: "world", text: "4.5 kg malachite · 31 kg granite rubble · pack 62% full" },
        { who: "agent", text: "report to @cody" },
    ],
}

export const open = {
    kicker: "Free and open",
    title: "Yours to play, fork and host.",
    lede: "Terraverse is free for everyone and always will be. The engine is LGPL, a fork of Luanti, and everything we build on it is in the open.",
}
