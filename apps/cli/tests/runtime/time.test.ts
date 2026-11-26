import { describe, expect, test } from "bun:test"
import { Channel } from "../../src/runtime/channel"
import { Running } from "../../src/runtime/running"
import { Call, Level, MemoryWire, Op } from "../../src/runtime/wire"
import { Time } from "../../src/runtime/api/game/time"

function setup() {
    const memory = MemoryWire()
    const channel = Channel({ wire: memory.wire })
    const running = Running({ channel: channel })
    const time = Time({ running: running, channel: channel })
    return { memory: memory, channel: channel, running: running, time: time }
}

describe("Time", () => {
    test("after fires in the window whose step reaches it, once", () => {
        const { time } = setup()
        const fired: number[] = []
        time.step(0)
        time.api.after({ seconds: 1 }, () => { fired.push(time.api.now) })
        time.step(0.5)
        expect(fired).toEqual([])
        time.step(1.2)
        time.step(5)
        expect(fired).toEqual([1.2])
    })

    test("every repeats, at most once a window", () => {
        const { time } = setup()
        let count = 0
        time.step(0)
        const stop = time.api.every({ seconds: 0.5 }, () => { count++ })
        time.step(0.5)
        time.step(3)    // a long step: one firing, not five
        expect(count).toBe(2)
        stop()
        time.step(10)
        expect(count).toBe(2)
    })

    test("timers set while loading count from the first window", () => {
        const { time } = setup()
        let fired = false
        time.api.after({ seconds: 1 }, () => { fired = true })
        time.step(100)      // a sidecar restarted under an old universe
        expect(fired).toBe(false)
        time.step(101)
        expect(fired).toBe(true)
    })

    test("wait resolves in the window that reaches it", async () => {
        const { time } = setup()
        time.step(0)
        let done = false
        const waiting = time.api.wait({ minutes: 1 }).then(() => { done = true })
        time.step(59)
        await Promise.resolve()
        expect(done).toBe(false)
        time.step(60)
        await waiting
        expect(done).toBe(true)
    })

    test("a throwing timer is reported under its mod, and the rest still run", () => {
        const { memory, running, time } = setup()
        let after = false
        time.step(0)
        running.run("@t/thrower", () => time.api.after({ seconds: 1 }, () => { throw new Error("boom") }))
        time.api.after({ seconds: 1 }, () => { after = true })
        time.step(1)
        expect(after).toBe(true)
        const log = memory.written().find(f => f.op === Op.log)
        expect(log?.payload).toEqual([Level.error, "@t/thrower", expect.stringContaining("boom")])
    })

    test("durations are checked", () => {
        const { time } = setup()
        expect(() => time.api.after({ seconds: -1 }, () => {})).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
        expect(() => time.api.every({ seconds: 0 }, () => {})).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
        expect(() => time.api.after({ seconds: 1 }, "soon" as never)).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
    })

    test("skip tells the engine, and settles when a window's clock is past it", async () => {
        const { memory, channel, time } = setup()
        time.step(100)
        let skipped: Promise<void> | undefined
        await channel.window(() => { skipped = time.api.skip({ hours: 8 }) })
        const told = memory.written().find(f => f.op === Op.call && Array.isArray(f.payload) && f.payload[0] === Call["time.skip"])
        expect(told?.payload).toEqual([Call["time.skip"], 8 * 3600])
        let done = false
        void skipped!.then(() => { done = true })
        time.step(101)
        await Promise.resolve()
        expect(done).toBe(false)
        time.step(100 + 8 * 3600)
        await skipped
        expect(done).toBe(true)
    })

    test("a skip is at least a second and at most a day", async () => {
        const { channel, time } = setup()
        await channel.window(() => {
            expect(() => time.api.skip({ seconds: 0 })).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
            expect(() => time.api.skip({ hours: 25 })).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
        })
    })
})
