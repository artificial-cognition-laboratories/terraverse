import type { Account, Backend, Game, Installed, Manifest, Message, Settings, Social, Version } from "./types.ts"
import { Refused } from "./types.ts"

// A pretend backend for building the page in a browser (`bun run ui`).
// `?scenario=` picks a situation to look at; see SCENARIOS.

const SCENARIOS = {
    normal: "signed in, one stable version installed, online",
    fresh: "first run: signed out, nothing installed (sign in to see the device code)",
    offline: "the manifest can't be fetched; a cached one is used",
    crash: "the game crashes a few seconds after launch",
    failing: "downloads fail part way",
    running: "the game is already running",
    update: "a newer launcher is ready",
} as const
type Scenario = keyof typeof SCENARIOS

function isScenario(name: string): name is Scenario {
    return name in SCENARIOS
}

const MB = 1024 * 1024
const GB = 1024 * MB

const VERSIONS: Version[] = [
    { id: "0.3.0-beta.2", channel: "beta", released: "2026-11-08", size: 412 * MB, protocol: 49,
        notes: "**Natural land.** Ridges and valleys no longer run in parallel lines across whole ranges.\n\n- Seams between drainage windows are gone: no more block-wide cliffs\n- Fewer tarns scattered over high ground\n- Trunk valleys bend and vary instead of combing a range's flank" },
    { id: "0.3.0-beta.1", channel: "beta", released: "2026-11-01", size: 409 * MB, protocol: 49,
        notes: "**Land shaped by water.** One drainage network at every scale, from the grid's rivers down to 150-block gullies.\n\n- Ridges, peaks and divides are where hillsides meet\n- Streams on their own floors; ponds behind sills\n- Hard rock stands steep, soft wears low" },
    { id: "0.2.0", channel: "stable", released: "2026-10-20", size: 380 * MB, protocol: 48,
        notes: "**Worn and watered.** Plates that wander, shields worn flat, rivers cut into the land.\n\n- Climate from currents and rain shadow\n- Gliders\n- Fire and gas in sealed rooms" },
    { id: "0.1.2", channel: "stable", released: "2026-10-04", size: 361 * MB, protocol: 47,
        notes: "**First public build.** Planets from a world code, matter and heat, driven machines." },
]

const CODY: Account = { status: "signedIn", id: "a1", username: "cody", name: "Cody" }
const FRIENDS: Social = {
    me: { state: "menu", world: null, open: false },
    joinable: false,
    view: {
        friends: [
            { id: "f1", username: "mara", name: "Mara", presence: { state: "hosting", world: "Tarn Valley", open: true } },
            { id: "f2", username: "jonno", name: null, presence: { state: "playing", world: "Basalt Coast", open: false } },
            { id: "f3", username: "wren", name: null, presence: null },
        ],
        incoming: [{ id: "f4", username: "pike", name: null }],
        outgoing: [],
        invites: [],
    },
}

const wait = (ms: number) => new Promise<void>(done => setTimeout(done, ms))

export function MockBackend(): Backend {
    const asked = new URLSearchParams(location.search).get("scenario") ?? "normal"
    if (!isScenario(asked)) throw new Error(`unknown scenario "${asked}": one of ${Object.keys(SCENARIOS).join(", ")}`)
    const scenario: Scenario = asked
    console.info(`[mock] ${scenario}: ${SCENARIOS[scenario]}`)

    let account: Account = scenario === "fresh" ? { status: "signedOut" } : CODY
    let social: Social | null = scenario === "fresh" ? null : structuredClone(FRIENDS)
    let settings: Settings = { beta: scenario !== "fresh", pinned: null, onLaunch: "hide", keepOld: true }
    const installed = new Map<string, Installed>(scenario === "fresh" ? [] : [
        ["0.2.0", { id: "0.2.0", size: 1.9 * GB }],
        ["0.3.0-beta.1", { id: "0.3.0-beta.1", size: 2.0 * GB }],
    ])
    const downloads = new Map<string, ReturnType<typeof setInterval>>()
    let game: Game = scenario === "running" ? { state: "running", version: "0.2.0", since: Date.now() - 23 * 60_000 } : { state: "idle" }
    const handlers = new Set<(message: Message) => void>()
    const send = (message: Message) => handlers.forEach(handler => handler(message))
    const setGame = (next: Game) => { game = next; send({ kind: "game", game: next }) }
    const manifest = (): Manifest => ({ versions: VERSIONS, fetched: Date.now() - (scenario === "offline" ? 3 * 86400_000 : 0) })
    const setAccount = (next: Account) => { account = next; send({ kind: "account", account: next }) }
    const setSocial = (next: Social | null) => { social = next; send({ kind: "social", social: next }) }
    type View = NonNullable<Social["view"]>
    // change the friends view, refusing while it isn't connected
    const changeView = (change: (view: View) => View) => {
        if (!social?.view) throw new Refused("Friends aren't connected.")
        setSocial({ ...social, view: change(social.view) })
    }
    let signing: ReturnType<typeof setTimeout> | undefined

    return {
        async about() {
            return { launcher: "0.1.0", dataDir: "/home/cody/.terraverse", platform: "linux-x86_64", free: 212 * GB }
        },

        async account() { return account },
        async signIn() {
            setAccount({ status: "starting" })
            await wait(600)
            setAccount({ status: "pending", code: "KX4-9QF", url: "https://arclabs.net/device" })
            // pretend the player approves it in the browser
            signing = setTimeout(() => {
                setAccount(CODY)
                setSocial(structuredClone(FRIENDS))
            }, 4000)
        },
        async cancelSignIn() {
            clearTimeout(signing)
            setAccount({ status: "signedOut" })
        },
        async retrySignIn() { setAccount(CODY) },
        async signOut() {
            if (game.state === "running") throw new Refused("Close the game before signing out.")
            setAccount({ status: "signedOut" })
            setSocial(null)
        },

        async social() { return social },
        async requestFriend(username) {
            changeView(v => {
                if (v.friends.some(f => f.username === username)) throw new Refused(`${username} is already a friend.`)
                return { ...v, outgoing: [...v.outgoing, { id: `o-${username}`, username, name: null }] }
            })
        },
        async acceptFriend(id) {
            changeView(v => {
                const player = v.incoming.find(p => p.id === id)
                if (!player) throw new Refused("That request has gone.")
                return { ...v, incoming: v.incoming.filter(p => p.id !== id),
                    friends: [...v.friends, { ...player, presence: { state: "menu", world: null, open: false } }] }
            })
        },
        async removeFriend(id) {
            changeView(v => ({ ...v, friends: v.friends.filter(f => f.id !== id),
                incoming: v.incoming.filter(p => p.id !== id), outgoing: v.outgoing.filter(p => p.id !== id) }))
        },

        async settings() { return settings },
        async setSettings(next) {
            if (next.pinned && !VERSIONS.some(v => v.id === next.pinned)) throw new Refused(`No version ${next.pinned}.`)
            settings = next
            return settings
        },

        async refreshManifest() {
            await wait(400)
            if (scenario === "offline") throw new Refused("Couldn't reach the release server.")
            return manifest()
        },
        async cachedManifest() { return scenario === "fresh" ? null : manifest() },

        async installed() { return [...installed.values()] },
        async install(id) {
            const version = VERSIONS.find(v => v.id === id)
            if (!version) throw new Refused(`No version ${id}.`)
            if (installed.has(id) || downloads.has(id)) throw new Refused(`${id} is already installed or installing.`)
            let received = 0
            downloads.set(id, setInterval(() => {
                received = Math.min(version.size, received + version.size / 50)
                if (scenario === "failing" && received > version.size * 0.6) {
                    clearInterval(downloads.get(id))
                    downloads.delete(id)
                    send({ kind: "installFailed", version: id, reason: "The download stopped: connection reset." })
                    return
                }
                send({ kind: "install", version: id, received, total: version.size })
                if (received < version.size) return
                clearInterval(downloads.get(id))
                downloads.delete(id)
                installed.set(id, { id, size: version.size * 4.8 })
                send({ kind: "installed", version: id })
            }, 100))
        },
        async cancel(id) {
            const timer = downloads.get(id)
            if (!timer) throw new Refused(`${id} isn't downloading.`)
            clearInterval(timer)
            downloads.delete(id)
        },
        async remove(id) {
            if (game.state === "running" && game.version === id) throw new Refused(`${id} is running.`)
            if (!installed.delete(id)) throw new Refused(`${id} isn't installed.`)
        },

        async game() { return game },
        async launch(id) {
            if (!installed.has(id)) throw new Refused(`${id} isn't installed.`)
            if (game.state === "running") throw new Refused("The game is already running.")
            setGame({ state: "running", version: id, since: Date.now() })
            setTimeout(() => setGame(scenario === "crash"
                ? { state: "crashed", version: id, session: "2026-11-10T10-41-07", code: 139 }
                : { state: "idle" }), scenario === "crash" ? 3000 : 10_000)
        },

        async collectReport(session) {
            await wait(300)
            return {
                session: session ?? "2026-11-10T09-12-55", version: "0.2.0", dump: session !== null,
                system: "Arch Linux, Wayland · NVIDIA RTX 2080 Ti (580.95) · 32 GB",
                log: "10:41:07 ACTION[Main]: Terraverse 0.2.0 (linux-x86_64)\n10:41:07 ACTION[Main]: world TF20.dGVycmE\n10:41:09 ACTION[Server]: cody joins\n10:41:10 ERROR[Main]: Segmentation fault in mesh_generator_thread",
            }
        },
        async submitReport() {
            await wait(900)
            if (game.state === "crashed") setGame({ state: "idle" })
        },
        async dismissCrash() { if (game.state === "crashed") setGame({ state: "idle" }) },

        async launcherUpdate() {
            return scenario === "update" ? { version: "0.1.1", notes: "Faster downloads." } : null
        },
        async updateLauncher() {
            if (game.state === "running") throw new Refused("Close the game first: the launcher restarts to update.")
            await wait(1500)
            location.reload() // as the real one restarts
        },

        async openDataFolder() { console.info("[mock] open the data folder") },
        async openUrl(url) { console.info(`[mock] open ${url}`) },

        listen(handler) {
            handlers.add(handler)
            return () => handlers.delete(handler)
        },
    }
}
