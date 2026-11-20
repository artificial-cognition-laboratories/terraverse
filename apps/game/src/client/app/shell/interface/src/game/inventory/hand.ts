// What the cursor is holding in the inventory screen, and what each click
// means. The hand only ever *points at* a stack the engine still owns in its
// slot; every change is an intent, and the slots update when the server
// confirms. Pure rules: no Vue, no DOM.

import type { Intents, SlotRef, Stack } from "../../bridge"

/** count null: "all of it", resolved against the live stack when placed. */
export type Held = { from: SlotRef; count: number | null }

export type Click = { slot: SlotRef; stack: Stack | null; button: "left" | "right"; shift: boolean }

export type Step =
    | { kind: "hold"; held: Held }
    | { kind: "release" }
    | { kind: "send"; intent: Outgoing; held: Held | null }
    | { kind: "none" }

export type Outgoing =
    | { kind: "inventory.move"; request: Intents["inventory.move"]["request"] }
    | { kind: "inventory.craft"; request: Intents["inventory.craft"]["request"] }
    | { kind: "inventory.drop"; request: Intents["inventory.drop"]["request"] }

const same = (a: SlotRef, b: SlotRef) => a.list === b.list && a.index === b.index

/** Lists whose shift-click sends the stack into the main inventory. */
const SHIFT_TO_MAIN = new Set(["craft", "craftresult", "worktop"])

export function click(held: Held | null, c: Click, liveCount: (slot: SlotRef) => number): Step {
    // The craft result is a button, not a slot: clicking it crafts.
    if (c.slot.list === "craftpreview") {
        if (held && held.from.list !== "craftresult") return { kind: "none" }
        const craft: Outgoing = { kind: "inventory.craft", request: { count: c.shift ? 64 : 1, collect: c.shift } }
        return { kind: "send", intent: craft, held: c.shift ? held : { from: { list: "craftresult", index: 0 }, count: null } }
    }

    if (!held) {
        if (!c.stack) return { kind: "none" }
        if (c.shift && SHIFT_TO_MAIN.has(c.slot.list)) {
            const move: Outgoing = { kind: "inventory.move", request: { from: c.slot, to: { list: "main" }, count: c.stack.count } }
            return { kind: "send", intent: move, held: null }
        }
        const count = c.button === "right" ? Math.ceil(c.stack.count / 2) : null
        return { kind: "hold", held: { from: c.slot, count: count } }
    }

    if (same(held.from, c.slot)) return { kind: "release" } // put it back
    const all = held.count ?? liveCount(held.from)
    if (all <= 0) return { kind: "release" }
    const count = c.button === "right" ? 1 : all
    const left = all - count
    const move: Outgoing = { kind: "inventory.move", request: { from: held.from, to: c.slot, count: count } }
    return { kind: "send", intent: move, held: left > 0 ? { from: held.from, count: left } : null }
}

/** A click outside the panel while holding: throw it on the ground. */
export function outside(held: Held | null, liveCount: (slot: SlotRef) => number): Step {
    if (!held) return { kind: "none" }
    const count = held.count ?? liveCount(held.from)
    if (count <= 0) return { kind: "release" }
    return { kind: "send", intent: { kind: "inventory.drop", request: { from: held.from, count: count } }, held: null }
}
