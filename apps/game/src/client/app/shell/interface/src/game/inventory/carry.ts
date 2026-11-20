// What the cursor carries across the inventory screens, and what every click,
// drag and key does with it, as Minecraft does it:
//
// - click a stack to pick it up (right: half), click a slot to put it down
//   (right: one), click outside the panel to throw it;
// - drag a stack and let go over a slot;
// - shift-click moves a stack between the hotbar and the rest at once;
// - hover a slot or a catalogue item and press 1–9: it goes to that hotbar slot;
// - in creative, the catalogue gives (left: a full stack, right: one,
//   shift: straight to the first empty slot), and clicking the catalogue or
//   the bin with a stack destroys it.
//
// The engine owns every stack: this only points at them and sends intents.
// What a move will do is shown at once (`lists`, predicted) and replaced by
// the server's answer when it comes, so nothing flickers while it's in flight.

import { computed, ref, shallowRef, watch } from "vue"
import { BridgeError, type BridgeHandle, type CreativeItem, type NodePos, type SlotRef, type Stack } from "../../bridge"
import type { GameStoreHandle } from "../store"
import { click, outside, type Held, type Outgoing, type Step } from "./hand"

const DRAG = 6 // px the mouse must move, button down, for a press to be a drag

/** Lists beside the player's own, each in a node's inventory (a station's worktop). */
export type NodeLists = Record<string, { width: number; stacks: (Stack | null)[]; at: NodePos }>

export function Carry(opts: { bridge: BridgeHandle; store: GameStoreHandle; nodes?: () => NodeLists | null }) {
    const { bridge, store } = opts
    const nodes = () => opts.nodes?.() ?? null
    const held = ref<Held | null>(null)                                   // a stack from a slot
    const given = ref<{ item: CreativeItem; count: number } | null>(null) // a stack from the catalogue
    const cursor = ref({ x: 0, y: 0 })
    const problem = ref<string | null>(null)
    let pressed: { x: number; y: number } | null = null

    type Lists = NonNullable<typeof store.state.inventory>
    const predicted = shallowRef<Lists | null>(null)
    // the server's next word on the inventory replaces any prediction
    watch([() => store.state.inventory, nodes], () => { predicted.value = null })
    const lists = computed<Lists | null>(() => {
        if (predicted.value) return predicted.value
        const own = store.state.inventory, more = nodes()
        if (!own || !more) return own
        const all: Lists = { ...own }
        for (const [name, list] of Object.entries(more)) all[name] = { width: list.width, stacks: list.stacks }
        return all
    })

    // A move as the server will make it: into an empty slot, onto the same
    // item, or swapped with what's there when the whole stack goes.
    function predict(intent: Outgoing) {
        const now = lists.value
        if (!now) return
        const next: Lists = {}
        for (const [name, list] of Object.entries(now)) next[name] = { width: list.width, stacks: [...list.stacks] }
        const take = (at: SlotRef, n: number) => {
            const s = next[at.list]?.stacks[at.index]
            if (!s) return
            next[at.list]!.stacks[at.index] = s.count > n ? { ...s, count: s.count - n } : null
        }
        if (intent.kind === "inventory.drop") take(intent.request.from, intent.request.count)
        if (intent.kind === "inventory.move" && intent.request.to.index !== undefined) {
            const { from, count } = intent.request
            const to = { list: intent.request.to.list, index: intent.request.to.index }
            const a = next[from.list]?.stacks[from.index], b = next[to.list]?.stacks[to.index]
            if (!a || !next[to.list]) return
            if (!b) { next[to.list]!.stacks[to.index] = { ...a, count: count }; take(from, count) }
            else if (b.item === a.item) { next[to.list]!.stacks[to.index] = { ...b, count: b.count + count }; take(from, count) }
            else if (count === a.count) { next[to.list]!.stacks[to.index] = a; next[from.list]!.stacks[from.index] = b }
            else return // a part onto something else: the server refuses; show nothing
        }
        if (intent.kind === "inventory.craft") return // the engine decides what a craft makes
        predicted.value = next
    }
    const hotbar = computed(() => store.state.hotbar?.size ?? 9)
    const live = (slot: SlotRef) => lists.value?.[slot.list]?.stacks[slot.index]?.count ?? 0

    const heldStack = computed<Stack | null>(() => {
        const h = held.value
        const stack = h ? lists.value?.[h.from.list]?.stacks[h.from.index] : null
        if (!h || !stack) return null
        return { ...stack, count: h.count ?? stack.count }
    })
    // the server emptied the slot the hand points at (moved, crafted, dropped): nothing to hold
    watch(heldStack, (stack) => { if (held.value && !stack) held.value = null })

    /** What the cursor draws. */
    const carried = computed<Stack | null>(() => given.value
        ? { item: given.value.item.item, count: given.value.count, wear: 0 }
        : heldStack.value)

    function fail(err: unknown) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
    }

    // a slot in a node's list says where the node is, for the engine
    function placed<T extends { list: string }>(slot: T): T {
        const at = nodes()?.[slot.list]?.at
        return at ? { ...slot, at: at } : slot
    }
    function send(intent: Outgoing) {
        switch (intent.kind) {
            case "inventory.move": return bridge.send(intent.kind, { ...intent.request, from: placed(intent.request.from), to: placed(intent.request.to) })
            case "inventory.craft": return bridge.send(intent.kind, intent.request)
            case "inventory.drop": return bridge.send(intent.kind, { ...intent.request, from: placed(intent.request.from) })
        }
    }

    async function apply(step: Step) {
        problem.value = null
        switch (step.kind) {
            case "hold": held.value = step.held; break
            case "release": held.value = null; break
            case "none": break
            case "send":
                held.value = step.held
                predict(step.intent)
                try { await send(step.intent) } catch (err) { fail(err); held.value = null; predicted.value = null }
        }
    }

    function give(item: string, slot: number, count: number) {
        const now = lists.value
        if (now?.main) { // shown at once, as the server will set it
            const stacks = [...now.main.stacks]
            stacks[slot] = { item: item, count: count, wear: 0 }
            predicted.value = { ...now, main: { width: now.main.width, stacks: stacks } }
        }
        bridge.send("ui.send", { name: "creative.take", payload: { item: item, slot: slot, count: count } }).catch(fail)
    }
    function destroy(slot: number) {
        const now = lists.value
        if (now?.main) {
            const stacks = [...now.main.stacks]
            stacks[slot] = null
            predicted.value = { ...now, main: { width: now.main.width, stacks: stacks } }
        }
        bridge.send("ui.send", { name: "creative.clear", payload: { slot: slot } }).catch(fail)
    }
    const firstEmpty = () => (lists.value?.main?.stacks ?? []).findIndex((s) => !s)

    /** A press on an inventory slot. */
    function slot(at: SlotRef, stack: Stack | null, button: "left" | "right", shift: boolean, event: MouseEvent) {
        if (given.value) { // a catalogue stack goes into the slot
            if (at.list === "main") give(given.value.item.item, at.index, given.value.count)
            given.value = null
            return
        }
        const main = lists.value?.main?.stacks
        if (shift && !held.value && at.list === "main" && stack && main) { // between the hotbar and the rest
            const inHotbar = at.index < hotbar.value
            const to = main.findIndex((s, i) => !s && (inHotbar ? i >= hotbar.value : i < hotbar.value))
            if (to >= 0) apply({ kind: "send", intent: { kind: "inventory.move", request: { from: at, to: { list: "main", index: to }, count: stack.count } }, held: null })
            return
        }
        if (!held.value && stack && !shift && at.list !== "craftpreview") pressed = { x: event.clientX, y: event.clientY }
        apply(click(held.value, { slot: at, stack: stack, button: button, shift: shift }, live))
    }

    /** A press on a catalogue item (creative). */
    function catalogue(item: CreativeItem, button: "left" | "right", shift: boolean, event: MouseEvent) {
        if (held.value) { // a stack dropped back on the catalogue is gone
            if (held.value.from.list === "main") destroy(held.value.from.index)
            held.value = null
            return
        }
        const count = button === "right" ? 1 : item.stackMax
        if (shift) {
            const to = firstEmpty()
            if (to >= 0) give(item.item, to, count)
            return
        }
        given.value = { item: item, count: count }
        pressed = { x: event.clientX, y: event.clientY }
    }

    /** The bin: whatever is carried is destroyed (creative). */
    function bin() {
        if (held.value?.from.list === "main") destroy(held.value.from.index)
        held.value = null
        given.value = null
    }

    /** A press outside the panel: throw what's carried. */
    function away() {
        given.value = null
        apply(outside(held.value, live))
    }

    /** 1–9 over a slot or catalogue item: to that hotbar slot. */
    function key(n: number, over: { slot: SlotRef; stack: Stack | null } | { item: CreativeItem } | null) {
        if (!over || n < 1 || n > hotbar.value) return
        const to = n - 1
        if ("item" in over) give(over.item.item, to, over.item.stackMax)
        else if (over.stack && !(over.slot.list === "main" && over.slot.index === to))
            apply({ kind: "send", intent: { kind: "inventory.move", request: { from: over.slot, to: { list: "main", index: to }, count: over.stack.count } }, held: null })
    }

    /** The mouse let go: a drag ends over whatever is under it. */
    function release(e: MouseEvent, under: Element | null) {
        const start = pressed
        pressed = null
        if (!start || Math.hypot(e.clientX - start.x, e.clientY - start.y) < DRAG || !under) return
        const target = under.closest<HTMLElement>("[data-list][data-index]")
        if (target) {
            const at = { list: target.dataset.list!, index: Number(target.dataset.index) }
            slot(at, lists.value?.[at.list]?.stacks[at.index] ?? null, "left", false, e)
        } else if (under.closest("[data-bin]")) {
            bin()
        } else if (!under.closest("[data-panel]")) {
            away()
        }
    }

    return {
        lists: lists,
        held: held,
        carried: carried,
        cursor: cursor,
        problem: problem,
        busy: computed(() => held.value !== null || given.value !== null),
        slot: slot,
        catalogue: catalogue,
        bin: bin,
        away: away,
        key: key,
        release: release,
        /** Put anything still carried back, as the screen closes. */
        drop() { held.value = null; given.value = null },
    }
}

export type CarryHandle = ReturnType<typeof Carry>
