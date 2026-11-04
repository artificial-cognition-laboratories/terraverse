// The wiki's air: the same blocks under different air, run by the world's own matter.
// What needs sealed rooms isn't built, and says so (spec: null, nothing plays).
// `hottest` and `left` are what bun check:wiki holds each to.

import type { SandboxBlock, SandboxSpec } from "../bridge"

export type AirDemo = {
    id: string; group: string; title: string; note: string; speed: number; spec: SandboxSpec | null
    hottest?: { min?: number; max?: number } // at the end of the run, not its peak
    active?: { max?: number } // W of reaction at the end
    left?: { id: string; min?: number; max?: number }
}

const ROOM = 293, ATM = 101325
const AIR = { nitrogen: 0.79, oxygen: 0.21 }
type Air = { pressure: number; air: Record<string, number> }
const EARTH: Air = { pressure: ATM, air: AIR }
const HIGH: Air = { pressure: 30000, air: AIR } // the top of a very high mountain
const VACUUM: Air = { pressure: 0, air: {} }

const floor = (n: number): SandboxBlock[] => {
    const out: SandboxBlock[] = []
    for (let x = 0; x < n; x++) for (let z = 0; z < n; z++) out.push({ at: [x, 0, z], node: "tf_world:stone" })
    return out
}
const scene = (size: [number, number, number], blocks: SandboxBlock[], air: Air, blows: SandboxSpec["blows"] = []): SandboxSpec =>
    ({ size: size, ambient: ROOM, pressure: air.pressure, air: air.air, blocks: blocks, blows: blows })
const log = (lit: number): SandboxBlock[] => [...floor(3), { at: [1, 1, 1], node: "tf_world:log", temperature: lit }]
const pot = (t: number): SandboxBlock[] => [{ at: [0, 0, 0], node: "tf_world:water_source", temperature: t, hold: true }]
const NOT_BUILT = "Not built yet: this needs sealed rooms, each with its own pressure and air."

export const AIR_DEMOS: AirDemo[] = [
    { id: "open", group: "Fire and air", title: "A fire in the open", speed: 10,
        note: "A log lit in ordinary air. It takes its oxygen from the room for as long as it burns.",
        spec: scene([3, 5, 3], log(900), EARTH), hottest: { min: 700 } },
    { id: "bellows", group: "Fire and air", title: "A bellows on a fire", speed: 10,
        note: "The same log with air pushed into it. More oxygen, faster burning.",
        spec: scene([3, 5, 3], log(900), EARTH, [{ at: [1, 1, 1], rate: 2, until: 600 }]), hottest: { min: 700 } },
    { id: "moon", group: "Thin air", title: "A fire with no air", speed: 10,
        note: "The same log at the same heat, in vacuum. With no oxygen it can't burn: it only cools down, whatever it's made of. It's why fire fails on the Moon.",
        spec: scene([3, 5, 3], log(900), VACUUM), active: { max: 1 } },
    { id: "boilhigh", group: "Thin air", title: "Boiling up a mountain", speed: 60,
        note: "Water held at 80 °C under a third of the air's pressure. That's hot enough to boil here, and steam comes off it.",
        spec: scene([1, 4, 1], pot(353), HIGH), left: { id: "water", max: 54000 } },
    { id: "boillow", group: "Thin air", title: "The same at sea level", speed: 60,
        note: "The same water at the same 80 °C under the full weight of the air. It only steams gently, and most of it stays.",
        spec: scene([1, 4, 1], pot(353), EARTH), left: { id: "water", min: 50000 } },
    { id: "smoke", group: "Bad air", title: "Smoke rising", speed: 10,
        note: "What a fire gives off is hot and lighter than the air, so it rises and leaves through the open top. Switch on heat to see it warm the blocks it passes.",
        spec: scene([3, 7, 3], log(900), EARTH), hottest: { min: 700 } },
    { id: "gunpowder", group: "Explosions", title: "Gunpowder in the open", speed: 1,
        note: "A fast reaction making hot gas. In the open the gas simply leaves, so all you get is the flash and the heat. Pressure and breaking walls need rooms and structure, which aren't built.",
        spec: scene([3, 3, 3], [...floor(3), { at: [1, 1, 1], node: "tf_matter:crucible", add: { potassium_nitrate: 20, carbon: 30, sulfur: 10 }, temperature: 800 }], EARTH), left: { id: "potassium_nitrate", max: 1 } },
    { id: "sealedfire", group: "Sealed rooms", title: "A fire in a sealed room", speed: 1, note: NOT_BUILT, spec: null },
    { id: "pressure", group: "Sealed rooms", title: "Pressure in a heated room", speed: 1, note: NOT_BUILT, spec: null },
    { id: "door", group: "Sealed rooms", title: "Opening a door on it", speed: 1, note: NOT_BUILT, spec: null },
    { id: "vent", group: "Sealed rooms", title: "Venting a mine", speed: 1, note: NOT_BUILT, spec: null },
    { id: "blast", group: "Sealed rooms", title: "An explosion in a room", speed: 1, note: `${NOT_BUILT} Walls breaking also needs structure.`, spec: null },
]
