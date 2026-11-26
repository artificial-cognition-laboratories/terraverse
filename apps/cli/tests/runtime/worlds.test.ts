import { describe, expect, test } from "bun:test"
import { Channel } from "../../src/runtime/channel"
import { Running } from "../../src/runtime/running"
import { Call, MemoryWire } from "../../src/runtime/wire"
import { Handlers } from "../../src/runtime/api/handlers"
import { Worlds } from "../../src/runtime/api/game/worlds"
import { Players, type Player } from "../../src/runtime/api/game/players"

// The engine played in memory: one player at `at`, whose time is `reading` there (null: seam).
function setup(opts: { at: readonly number[] | null; reading: unknown[] | null }) {
    const memory = MemoryWire({
        answer: (call) => {
            if (call === Call["players.list"]) return ["@ana"]
            if (call === Call["player.position"]) return opts.at
            if (call === Call["world.time"]) return opts.reading
            return null
        },
    })
    const channel = Channel({ wire: memory.wire })
    const running = Running({ channel: channel })
    const players = Players({ channel: channel, handlers: Handlers({ running: running }), worlds: Worlds({ channel: channel }) })
    /** fn, with @ana, inside a window. */
    const within = (fn: (ana: Player) => void) =>
        channel.window(() => {
            players.step({ now: 0, dt: 0, happened: [] })
            fn(players.api.get("@ana")!)
        })
    return { within: within }
}

// a 1200 s day, 14:00, the sun up half of it: dawn at 06:00, dusk at 18:00
const AFTERNOON = ["home", 3, 41, 14, 0.5, "summer", 1200]

describe("Worlds", () => {
    test("a player's world is the one they stand on, held as one handle", async () => {
        const { within } = setup({ at: [10, 64, -3], reading: AFTERNOON })
        await within(ana => {
            expect(ana.position).toEqual([10, 64, -3])
            expect(ana.world.name).toBe("home")
            expect(ana.world).toBe(ana.world)
        })
    })

    test("a calendar reads the time at a place", async () => {
        const { within } = setup({ at: [0, 0, 0], reading: AFTERNOON })
        await within(ana => {
            const now = ana.world.calendar.at(ana.position)
            expect([now.year, now.day, now.hour, now.season, now.dayLength]).toEqual([3, 41, 14, "summer", 1200])
        })
    })

    test("until counts forward to the next moment, in game seconds", async () => {
        const { within } = setup({ at: [0, 0, 0], reading: AFTERNOON })
        await within(ana => {
            const now = ana.world.calendar.at(ana.position)
            expect(now.until("dusk")).toBe(200)        // 4 h of a 24 h day of 1200 s
            expect(now.until("dawn")).toBe(800)        // 16 h on, round midnight
            expect(now.until(14)).toBe(1200)           // now is a whole day away
            expect(() => now.until(24)).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
        })
    })

    test("where the sun never sets, dawn and dusk never come", async () => {
        const { within } = setup({ at: [0, 0, 0], reading: ["home", 1, 170, 3, 1, "summer", 1200] })
        await within(ana => {
            const now = ana.world.calendar.at(ana.position)
            expect(now.until("dawn")).toBeNull()
            expect(now.until("noon")).toBe(450)
        })
    })

    test("where the sun never moves, nothing comes", async () => {
        const { within } = setup({ at: [0, 0, 0], reading: ["moon", 1, 1, 9, 1, "spring", 0] })
        await within(ana => {
            expect(ana.world.calendar.at(ana.position).dayLength).toBeNull()
            expect(ana.world.calendar.at(ana.position).until("noon")).toBeNull()
        })
    })

    test("off every world, or not yet placed, says so", async () => {
        const seam = setup({ at: [0, 0, 0], reading: null })
        await seam.within(ana => { expect(() => ana.world).toThrow(expect.objectContaining({ code: "NOT_PLACED" })) })
        const joining = setup({ at: null, reading: AFTERNOON })
        await joining.within(ana => { expect(() => ana.position).toThrow(expect.objectContaining({ code: "NOT_PLACED" })) })
    })

    test("what isn't wired says so", async () => {
        const { within } = setup({ at: [0, 0, 0], reading: AFTERNOON })
        await within(ana => {
            expect(() => (ana.world as unknown as { spawn: unknown }).spawn).toThrow(expect.objectContaining({ code: "NOT_WIRED" }))
        })
    })
})
