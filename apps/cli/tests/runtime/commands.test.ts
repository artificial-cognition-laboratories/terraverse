import { describe, expect, test } from "bun:test"
import { Channel, type Happening } from "../../src/runtime/channel"
import { Running } from "../../src/runtime/running"
import { Call, Level, MemoryWire, Op } from "../../src/runtime/wire"
import { Handlers } from "../../src/runtime/api/handlers"
import { Worlds } from "../../src/runtime/api/game/worlds"
import { Players } from "../../src/runtime/api/game/players"
import { Commands } from "../../src/runtime/api/game/commands"
import { Refused } from "../../src/runtime/api/refused"

/** Players @ana (who may do anything) and @bo (who may do nothing) are here. */
function setup() {
    const memory = MemoryWire({
        answer: (call, args) => {
            if (call === Call["players.list"]) return ["@ana", "@bo"]
            if (call === Call["player.can"]) return args[0] === "@ana"
            return null
        },
    })
    const channel = Channel({ wire: memory.wire })
    const running = Running({ channel: channel })
    const players = Players({ channel: channel, handlers: Handlers({ running: running }), worlds: Worlds({ channel: channel }) })
    const commands = Commands({ channel: channel, running: running, players: players.api })
    const step = (happened: Happening[] = []) => channel.window(() => {
        players.step({ now: 0, dt: 0, happened: happened })
        commands.step(happened)
    })
    const typed = (player: string, command: string, text = "") => step([{ kind: "command", player: player, command: command, text: text }])
    const calls = (call: number) => memory.written().filter(f => f.op === Op.call && Array.isArray(f.payload) && f.payload[0] === call).map(f => f.payload)
    const told = (player: string) => calls(Call["player.tell"]).filter(p => Array.isArray(p) && p[1] === player).map(p => Array.isArray(p) ? p[2] : null)
    return { memory: memory, commands: commands, step: step, typed: typed, calls: calls, told: told }
}

describe("Commands", () => {
    test("the engine is told each command by its path, with what it takes and who may, and again when they change", async () => {
        const { commands, step, calls } = setup()
        commands.api.create("kiln light", { description: "Light it", permission: "kilns:light", run() {} })
        const off = commands.api.create("kiln count", { description: "How many", run() {} })
        commands.api.create("msg", { description: "Say", args: { to: { type: "player" }, text: { type: "string" } }, run() {} })
        commands.api.create("heat", { description: "Set", args: { how: { type: "string", options: ["low", "high"], optional: true } }, run() {} })
        await step()
        expect(calls(Call["commands.set"]).at(-1)).toEqual([Call["commands.set"], [
            ["kiln light", "Light it", "", "kilns:light", []],
            ["kiln count", "How many", "", "", []],
            ["msg", "Say", "<to> <text>", "", [["to", "player", false, [], "", []], ["text", "string", false, [], "", []]]],
            ["heat", "Set", "[low|high]", "", [["how", "string", true, ["low", "high"], "", []]]],
        ]])
        await step()
        expect(calls(Call["commands.set"]).length).toBe(1)
        off()
        await step()
        expect(calls(Call["commands.set"]).at(-1)).toEqual([Call["commands.set"], [
            ["kiln light", "Light it", "", "kilns:light", []],
            ["msg", "Say", "<to> <text>", "", [["to", "player", false, [], "", []], ["text", "string", false, [], "", []]]],
            ["heat", "Set", "[low|high]", "", [["how", "string", true, ["low", "high"], "", []]]],
        ]])
    })

    test("an argument's description and suggestions reach the engine, for the palette", async () => {
        const { commands, step, calls } = setup()
        commands.api.create("time", {
            description: "The time",
            args: { time: { type: "string", optional: true, description: "When", suggest: ["dawn", { value: "noon", description: "12:00" }] } },
            run() {},
        })
        await step()
        expect(calls(Call["commands.set"]).at(-1)).toEqual([Call["commands.set"], [
            ["time", "The time", "[<time>]", "", [["time", "string", true, [], "When", [["dawn", ""], ["noon", "12:00"]]]]],
        ]])
    })

    test("suggestions are words: one with a space in it is refused", () => {
        const { commands } = setup()
        expect(() => commands.api.create("time", {
            description: "The time",
            args: { time: { type: "string", suggest: ["high noon"] } },
            run() {},
        })).toThrow(expect.objectContaining({ code: "ARGUMENT_INVALID" }))
    })

    test("a line is parsed into typed args, the last string taking the rest", async () => {
        const { commands, typed } = setup()
        let got: unknown
        commands.api.create("msg", {
            description: "Say",
            args: { to: { type: "player" }, text: { type: "string" } },
            run({ args }) { got = { to: Reflect.get(Object(args.to), "name"), text: args.text } },
        })
        await typed("@bo", "msg", "ana  hello there, ana")
        expect(got).toEqual({ to: "@ana", text: "hello there, ana" })
    })

    test("typed wrong, it never runs, and they're told what it takes", async () => {
        const { commands, typed, told } = setup()
        let ran = false
        commands.api.create("heat", { description: "Heat", args: { k: { type: "integer", min: 300, max: 2000 } }, run() { ran = true } })
        await typed("@bo", "heat", "12.5")
        await typed("@bo", "heat", "100")
        await typed("@bo", "heat")
        expect(ran).toBe(false)
        expect(told("@bo")).toEqual([
            "k is a whole number, not 12.5. Usage: /heat <k>",
            "k is from 300 to 2000. Usage: /heat <k>",
            "Missing k. Usage: /heat <k>",
        ])
    })

    test("a permission is checked before it runs; the console may run anything", async () => {
        const { commands, typed, told, memory } = setup()
        const ran: string[] = []
        commands.api.create("kick", { description: "Kick", permission: "commands:kick", run({ player }) { ran.push(player?.name ?? "console") } })
        await typed("@bo", "kick")
        await typed("@ana", "kick")
        await typed("", "kick")
        expect(ran).toEqual(["@ana", "console"])
        expect(told("@bo")).toEqual(["You may not: /kick needs commands:kick."])
        expect(memory.written().some(f => f.op === Op.log)).toBe(false)
    })

    test("Refused is the reply; anything else is logged, and they're told it failed", async () => {
        const { commands, typed, told, memory } = setup()
        commands.api.create("no", { description: "No", run() { throw new Refused("Not now.") } })
        commands.api.create("oops", { description: "Oops", async run() { throw new Error("broke") } })
        await typed("@ana", "no")
        await typed("@ana", "oops")
        expect(told("@ana")).toEqual(["Not now.", "/oops failed: what went wrong is in the server's log."])
        const logged = memory.written().filter(f => f.op === Op.log).map(f => f.payload)
        expect(logged).toContainEqual([Level.error, "", expect.stringContaining("broke")])
    })

    test("a group lists what's under it; a path is taken once", async () => {
        const { commands, typed, told } = setup()
        commands.api.create(["kiln", "light"], { description: "Light it", args: { heat: { type: "number", optional: true } }, run() {} })
        await typed("@ana", "kiln")
        expect(told("@ana")).toEqual(["/kiln:\n  /kiln light [<heat>] — Light it"])
        expect(() => commands.api.create("kiln light", { description: "Again", run() {} })).toThrow(expect.objectContaining({ code: "COMMAND_TAKEN" }))
    })
})
