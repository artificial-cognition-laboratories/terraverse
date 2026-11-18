import { describe, expect, test } from "bun:test"
import { Channel } from "../../src/runtime/channel"
import { Running } from "../../src/runtime/running"
import { Call, MemoryWire, Op } from "../../src/runtime/wire"
import { Handlers } from "../../src/runtime/api/handlers"
import { Players, type Player } from "../../src/runtime/api/game/players"

function setup(here: string[] = []) {
    const memory = MemoryWire({ answer: call => (call === Call["players.list"] ? here : null) })
    const channel = Channel({ wire: memory.wire })
    const running = Running({ channel: channel })
    const players = Players({ channel: channel, handlers: Handlers({ running: running }) })
    const step = (happened: { kind: "join" | "leave"; player: string }[]) =>
        channel.window(() => players.step({ now: 0, dt: 0, happened: happened }))
    return { memory: memory, players: players, step: step }
}

describe("Players", () => {
    test("whoever was here before the first window is here, without a join", async () => {
        const { players, step } = setup(["@cody"])
        const joined: string[] = []
        players.api.on("join", p => { joined.push(p.name) })
        await step([])
        expect(players.api.list().map(p => p.name)).toEqual(["@cody"])
        expect(joined).toEqual([])
    })

    test("a join reaches every handler, and tell goes to the engine", async () => {
        const { memory, players, step } = setup()
        players.api.on("join", p => p.tell(`Hello, ${p.name}.`))
        await step([{ kind: "join", player: "@ana" }])
        const told = memory.written().find(f => f.op === Op.call && Array.isArray(f.payload) && f.payload[0] === Call["player.tell"])
        expect(told?.payload).toEqual([Call["player.tell"], "@ana", "Hello, @ana."])
    })

    test("a player held after they leave says so", async () => {
        const { players, step } = setup()
        let held: Player | undefined
        players.api.on("join", p => { held = p })
        await step([{ kind: "join", player: "@ana" }])
        await step([{ kind: "leave", player: "@ana" }])
        expect(held?.connected).toBe(false)
        expect(players.api.get("@ana")).toBeUndefined()
        await step([])
        expect(() => held?.tell("still there?")).toThrow(expect.objectContaining({ code: "PLAYER_GONE" }))
    })

    test("a disposed handler isn't called", async () => {
        const { players, step } = setup()
        let calls = 0
        const stop = players.api.on("join", () => { calls++ })
        stop()
        await step([{ kind: "join", player: "@ana" }])
        expect(calls).toBe(0)
    })

    test("nothing can be asked outside a window", () => {
        const { players } = setup()
        expect(() => players.step({ now: 0, dt: 0, happened: [] })).toThrow(expect.objectContaining({ code: "NOT_RUNNING" }))
    })
})
