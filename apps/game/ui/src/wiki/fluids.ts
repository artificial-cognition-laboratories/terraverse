// The wiki's liquids and powders: real blocks and real pours, run by the world's
// own flow (src/sandbox/pouring.cpp). `wet` is what bun check:wiki holds each to:
// how full a block must end, of its height or its open space.

import type { SandboxBlock, SandboxSpec } from "../bridge"
import type { LabDemo } from "./demos.ts"

type At = [number, number, number]
export type FluidDemo = LabDemo & { wet: { at: At; min?: number; max?: number }[] }

const ROOM = 293, ATM = 101325
const AIR = { nitrogen: 0.79, oxygen: 0.21 }
const STONE = "tf_world:stone", BRICK = "tf_world:stone_bricks", WATER = "tf_world:water_source"
const MOLES_M3_WATER = 55500 // moles of water in a cubic metre

const span = (from: At, to: At, node: string): SandboxBlock[] => {
    const out: SandboxBlock[] = []
    for (let y = from[1]; y <= to[1]; y++) for (let z = from[2]; z <= to[2]; z++) for (let x = from[0]; x <= to[0]; x++) out.push({ at: [x, y, z], node: node })
    return out
}
// Walls `high` round the edge of a floor `x` by `z`, leaving out what `gap` says.
const pit = (x: number, z: number, high: number, gap: (x: number, y: number, z: number) => boolean = () => false): SandboxBlock[] => {
    const out = span([0, 0, 0], [x - 1, 0, z - 1], STONE)
    for (let y = 1; y <= high; y++) for (let k = 0; k < z; k++) for (let i = 0; i < x; i++)
        if ((i === 0 || k === 0 || i === x - 1 || k === z - 1) && !gap(i, y, k)) out.push({ at: [i, y, k], node: BRICK })
    return out
}
const scene = (size: At, blocks: SandboxBlock[], pours: SandboxSpec["pours"] = []): SandboxSpec =>
    ({ size: size, ambient: ROOM, pressure: ATM, air: AIR, blocks: blocks, pours: pours })

export const FLUID_DEMOS: FluidDemo[] = [
    {
        id: "pour", group: "Water", title: "Filling a pit", speed: 10,
        note: "Poured from above, water lands, spreads to the walls and rises level. What you see is how much is really in each block, to the sixteenth.",
        spec: scene([7, 5, 7], pit(7, 7, 2), [{ at: [3, 4, 3], substance: "water", rate: MOLES_M3_WATER * 0.5, until: 40 }]),
        wet: [{ at: [1, 1, 1], min: 0.3 }, { at: [5, 1, 5], min: 0.3 }],
    },
    {
        id: "dam", group: "Water", title: "A column let go", speed: 1,
        note: "Blocks of still water with nothing holding them. They run out and flatten, carrying their momentum across the floor.",
        spec: scene([9, 5, 9], [...pit(9, 9, 1), ...span([3, 1, 3], [5, 3, 5], WATER)]),
        wet: [{ at: [1, 1, 1], min: 0.05 }, { at: [7, 1, 7], min: 0.05 }, { at: [4, 3, 4], max: 0.01 }],
    },
    {
        id: "level", group: "Water", title: "Two pools find one level", speed: 10,
        note: "A full basin and an empty one, joined low down. Water runs through till both stand at the same height.",
        spec: scene([11, 5, 5], [
            ...pit(11, 5, 3),
            ...span([5, 2, 1], [5, 3, 3], BRICK),
            ...span([1, 1, 1], [4, 3, 3], WATER),
        ]),
        wet: [{ at: [8, 1, 2], min: 0.5 }, { at: [2, 1, 2], min: 0.5 }],
    },
    {
        id: "overflow", group: "Water", title: "Over the lip, down the steps", speed: 10,
        note: "A basin fills to its lowest edge, spills, and runs down whatever is below it, pool by pool.",
        spec: scene([5, 7, 14], [
            ...span([0, 0, 0], [4, 0, 13], STONE),
            ...span([0, 1, 0], [4, 3, 5], STONE),
            ...span([0, 1, 6], [4, 2, 8], STONE),
            ...span([0, 1, 9], [4, 1, 11], STONE),
            ...span([0, 4, 0], [4, 4, 0], BRICK), ...span([0, 4, 1], [0, 4, 5], BRICK), ...span([4, 4, 1], [4, 4, 5], BRICK),
        ], [{ at: [2, 6, 2], substance: "water", rate: MOLES_M3_WATER * 0.6, until: 120 }]),
        wet: [{ at: [2, 2, 12], min: 0.02 }],
    },
    {
        id: "copper", group: "Other liquids", title: "A copper pour", speed: 1,
        note: "Molten copper poured into a stone mould. It runs like water, but it's losing heat to the stone the whole time, and it freezes where it's lying.",
        spec: scene([5, 5, 5], pit(5, 5, 1), [{ at: [2, 4, 2], substance: "copper", rate: 8000, temperature: 1500, until: 15 }]),
        wet: [],
    },
    {
        id: "onwater", group: "Other liquids", title: "Hot metal into water", speed: 1,
        note: "The same pour into a pool. Whatever touches it boils, and the metal sets far sooner than it did in air.",
        spec: scene([5, 6, 5], [...pit(5, 5, 2), ...span([1, 1, 1], [3, 1, 3], WATER)], [{ at: [2, 5, 2], substance: "copper", rate: 8000, temperature: 1500, until: 10 }]),
        wet: [],
    },
    {
        id: "spirit", group: "Other liquids", title: "A spirit spill", speed: 10,
        note: "Ethanol on a warm floor. Open surfaces lose it to the air, so a thin spill is gone long before a deep one.",
        spec: scene([7, 4, 7], pit(7, 7, 1), [{ at: [3, 3, 3], substance: "ethanol", rate: 4000, until: 4 }]),
        wet: [],
    },
    {
        id: "salt", group: "Powders", title: "A heap of salt", speed: 10,
        note: "A powder doesn't level out. It stacks up until the slope is too steep to hold, then slides: a cone at its angle of repose.",
        spec: scene([11, 7, 11], span([0, 0, 0], [10, 0, 10], STONE), [{ at: [5, 6, 5], substance: "salt", rate: 12000, until: 90 }]),
        wet: [{ at: [5, 1, 5], min: 0.9 }, { at: [5, 2, 5], min: 0.05 }],
    },
    {
        id: "sulfur", group: "Powders", title: "Against a wall", speed: 10,
        note: "Poured beside a wall, a powder leans on it and slopes away on the open side only.",
        spec: scene([9, 7, 7], [...span([0, 0, 0], [8, 0, 6], STONE), ...span([0, 1, 0], [0, 5, 6], BRICK)], [{ at: [1, 6, 3], substance: "sulfur", rate: 12000, until: 60 }]),
        wet: [{ at: [1, 1, 3], min: 0.9 }],
    },
    {
        id: "funnel", group: "Powders", title: "Through a gap", speed: 10,
        note: "A heap on a ledge feeds through a hole, slumping in from every side, and builds a new heap below.",
        spec: scene([7, 8, 7], [
            ...span([0, 0, 0], [6, 0, 6], STONE),
            ...span([0, 3, 0], [6, 3, 6], BRICK).filter((b) => !(b.at[0] === 3 && b.at[2] === 3)),
        ], [{ at: [3, 7, 3], substance: "salt", rate: 10000, until: 60 }]),
        wet: [{ at: [3, 1, 3], min: 0.5 }],
    },
]
