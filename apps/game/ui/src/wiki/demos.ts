// The wiki's demos as sandbox specs: one generated for each reaction from
// its own numbers, and the Lab's hand-made ones for physical behaviour.
// Nothing here simulates: the engine does (src/sandbox).

import type { MatterReaction, MatterState, MatterSubstance, SandboxBlock, SandboxSpec } from "../bridge"

export const AIR: Record<string, number> = { nitrogen: 0.79, oxygen: 0.21 }
const ROOM = 293.15
const ATM = 101325
const R = 8.314462618

/** A substance's state at 1 atm and temperature t, from its anchors (the engine's phase model, simplified to one pressure). */
export function stateAt(s: MatterSubstance, t: number): MatterState {
    if (s.boil !== null && t >= s.boil) return "gas"
    if (s.sublime !== undefined && t >= s.sublime) return "gas"
    if (s.boil === null && s.sublime === undefined && t >= s.critical[0]) return "gas"
    return t >= s.triple[0] ? "liquid" : "solid"
}

export type Demo =
    | { runnable: true; spec: SandboxSpec; temperature: number; note: string }
    | { runnable: false; reason: string }

/**
 * A reaction's demo, from its own numbers: the reactants measured by the
 * equation and put together as a furnace or a flask would, in a 3 × 3 × 3
 * box of room air.
 *
 * - Temperature: where it runs at a pace you can watch (k = 0.2/s), kept
 *   where the liquids it needs are liquid, held there by a heater. (Some, like
 *   rusting, are slow even then: the speed control shows them over hours.)
 * - Solid and liquid reactants share one block; the gas reactants fill the
 *   space around them, in proportion (ore with its carbon monoxide).
 * - A reaction of gases alone fills a few blocks with the mixture, and one
 *   that gives off heat gets a spark.
 */
export function reactionDemo(r: MatterReaction, substances: Map<string, MatterSubstance>): Demo {
    if (r.energy)
        return { runnable: false, reason: `It runs on ${r.energy}: the sandbox has no ${r.energy === "electricity" ? "current" : "lamp"} yet.` }
    const sub = (id: string) => {
        const s = substances.get(id)
        if (!s) throw new Error(`reaction ${r.id} names ${id}, which matter.list doesn't have`)
        return s
    }
    // where it runs at a visible pace (when its numbers allow one), and above any melting it needs
    let t = ROOM
    if (r.activation > 0 && r.factor > 0.2) t = r.activation * 1000 / (R * Math.log(r.factor / 0.2))
    // a liquid it needs must be liquid: above its melting point, below its boiling point
    for (const q of r.requires ?? [])
        if (q.state === "liquid") t = Math.max(t, sub(q.substance).triple[0] + 40)
    for (const q of r.requires ?? []) {
        const boil = sub(q.substance).boil
        if (q.state === "liquid" && boil !== null) t = Math.min(t, boil - 10)
    }
    if (!Number.isFinite(t)) throw new Error(`reaction ${r.id}: no temperature for its demo`)
    t = Math.max(ROOM, Math.min(3000, t))

    const condensed: [string, number][] = [], gases: [string, number][] = []
    for (const [id, count] of Object.entries(r.reactants)) {
        const s = sub(id)
        const state = stateAt(s, t)
        if (state === "gas") gases.push([id, count])
        else condensed.push([id, count * s.molar_mass / (state === "liquid" ? s.density.liquid : s.density.solid)])
    }
    // what it needs present without using up (rusting's water) goes in too, a share of the block
    for (const q of r.requires ?? [])
        if (!(q.substance in r.reactants) && q.state !== "gas") {
            const s = sub(q.substance)
            const v = condensed.reduce((a, [, x]) => a + x, 0) || s.molar_mass / s.density.liquid
            condensed.push([q.substance, v * 0.3])
        }
    const shares = (list: [string, number][], scale: number) => {
        const total = list.reduce((a, [, v]) => a + v, 0)
        return Object.fromEntries(list.map(([id, v]) => [id, scale * v / total]))
    }
    // in a crucible, as a player would: its charge by the equation, about a third of a block's room
    const add: Record<string, number> = {}
    const volume = condensed.reduce((total, [, v]) => total + v, 0)
    for (const [id, v] of condensed) add[id] = (0.15 * v / volume) / (sub(id).molar_mass / 1000 / sub(id).density.solid)
    for (const [id, count] of gases) add[id] = count * 20
    const blocks: SandboxBlock[] = [{ at: [1, 0, 1], node: "tf_matter:crucible", add: add,
        // gases alone that burn: a spark, then their own heat
        temperature: !condensed.length && r.heat > 0 ? Math.max(t, 1100) : t, hold: condensed.length > 0 || r.heat <= 0 }]
    const heat = r.heat > 0 ? "gives off heat" : "takes in heat"
    return {
        runnable: true,
        temperature: t,
        spec: { size: [3, 3, 3], ambient: ROOM, pressure: ATM, air: AIR, blocks: blocks },
        note: `Its reactants, measured by the equation, at ${Math.round(t)} K, where it runs at a pace you can watch${condensed.length ? "; a heater holds them there" : ""}. It ${heat}.`,
    }
}

/** A physical demo for the Lab: what it shows, and the box that shows it. */
export type LabDemo = { id: string; group: string; title: string; note: string; spec: SandboxSpec; speed: number }

const box = (size: [number, number, number], blocks: SandboxBlock[], air = AIR): SandboxSpec =>
    ({ size: size, ambient: ROOM, pressure: ATM, air: air, blocks: blocks })
const floor = (y = 0): SandboxBlock[] => [0, 1, 2].flatMap((x) => [0, 1, 2].map((z): SandboxBlock => ({ at: [x, y, z], node: "tf_world:stone" })))

const BURNER = 6e6 // W a block of the pot gets: a metre cube of water warms in about a minute

export const LAB_DEMOS: LabDemo[] = [
    {
        id: "ice", group: "Heat", title: "Ice melts", speed: 1,
        note: "A block of ice over a burner. It warms to 0 °C, then stays there while the heat goes into melting it, and the water runs out and pools. In a warm room alone, a block this size would take days.",
        spec: box([3, 2, 3], [...floor(), { at: [1, 1, 1], node: "tf_world:ice", temperature: 258, power: 5e6 }]),
    },
    {
        id: "boil", group: "Heat", title: "Water boils", speed: 1,
        note: "A red-hot floor under a pot. It warms for a while, with a few bubbles that die on the way up, then a rolling boil that holds at 100 °C for as long as there is water. The steam is clear where it leaves, and shows as mist where it cools.",
        spec: box([5, 8, 5], [0, 1, 2, 3, 4].flatMap((x) => [0, 1, 2, 3, 4].flatMap((z): SandboxBlock[] => {
            const wall = x === 0 || z === 0 || x === 4 || z === 4
            return [
                wall ? { at: [x, 0, z], node: "tf_world:stone" } : { at: [x, 0, z], node: "tf_world:stone", temperature: 1000, hold: true },
                ...[1, 2].map((y): SandboxBlock => wall ? { at: [x, y, z], node: "tf_world:stone_bricks" } : { at: [x, y, z], node: "tf_world:water_source", power: BURNER }),
            ]
        }))),
    },
    {
        id: "conduct", group: "Heat", title: "Copper and wood, heated at one end", speed: 600,
        note: "The same heat into the end of each bar. These are bars a metre thick, so even copper takes an hour to carry it a few blocks; wood barely moves it at all. Turn on the heat view to watch it go.",
        spec: box([2, 1, 4], [
            ...[0, 1, 2, 3].flatMap((z): SandboxBlock[] => [{ at: [0, 0, z], node: "tf_matter:copper" }, { at: [1, 0, z], node: "tf_world:log" }]),
            { at: [0, 0, 0], node: "tf_matter:copper", temperature: 700, hold: true }, { at: [1, 0, 0], node: "tf_world:log", temperature: 700, hold: true },
        ]),
    },
    {
        id: "sublime", group: "Heat", title: "Dry ice", speed: 600,
        note: "Solid carbon dioxide at 1 atm never melts: it turns straight to gas at −78 °C, and the heavy cold gas spills out over the top.",
        spec: box([3, 2, 3], [{ at: [1, 0, 1], node: "tf_matter:carbon_dioxide", temperature: 190 }]),
    },
    {
        id: "pour", group: "Heat", title: "Tin melts and pours", speed: 10,
        note: "Tin melts at 232 °C, low for a metal. Heated on a shelf, it melts and runs down into the space below.",
        spec: box([1, 3, 1], [{ at: [0, 2, 0], node: "tf_matter:tin", temperature: 560, hold: true }]),
    },
    {
        id: "fire", group: "Fire", title: "Wood burns in air", speed: 1,
        note: "Lit wood burns where air reaches it: the fire draws oxygen in and sends carbon dioxide and steam up and out of the open top. Its own heat keeps it going.",
        spec: box([3, 3, 3], [...floor(), { at: [1, 1, 1], node: "tf_world:log", temperature: 900 }]),
    },
    {
        id: "kiln", group: "Fire", title: "Wood heated without air", speed: 10,
        note: "The same wood, as hot, but in nitrogen: with no oxygen it can't burn. It chars instead, giving off steam and leaving charcoal: a charcoal kiln.",
        spec: box([3, 3, 3], [...floor(), { at: [1, 1, 1], node: "tf_world:log", temperature: 900, hold: true }], { nitrogen: 1 }),
    },
    {
        id: "bronze", group: "Metals", title: "Bronze from copper and tin", speed: 10,
        note: "Copper and tin heated together: once both are molten they mix into bronze, harder than either.",
        spec: box([3, 2, 3], [{ at: [1, 0, 1], node: "tf_matter:crucible", add: { copper: 2300, tin: 150 }, temperature: 1400, hold: true }]), // 15 : 1 by moles
    },
]
