import type { Capability, Catalogue } from "./making.ts"

/**
 * TEMP(2026-11-23): making's first catalogue, for the feel test only. Rough real values; the
 * engine's table (assets/matter) replaces it once the design holds. The game has no sticks or
 * pebbles yet, so its blocks stand in: a dead bush is sticks, flint ore a flint, reeds stalks.
 */
export const CATALOGUE: Catalogue = {
    materials: {
        oak: { id: "oak", name: "Oak", working: "fibrous", hardness: 3, colour: "#8a6236" },
        birch: { id: "birch", name: "Birch", working: "fibrous", hardness: 2.6, colour: "#d8cfb4" },
        spruce: { id: "spruce", name: "Spruce", working: "fibrous", hardness: 2.2, colour: "#a5784a" },
        deadwood: { id: "deadwood", name: "Dry wood", working: "fibrous", hardness: 2.4, colour: "#9c7b52" },
        flint: { id: "flint", name: "Flint", working: "brittle", hardness: 7, colour: "#5b616b" },
        granite: { id: "granite", name: "Granite", working: "granular", hardness: 6, colour: "#a59a92" },
        reed: { id: "reed", name: "Reed", working: "fibre", hardness: 1, colour: "#8fa65a" },
        grass: { id: "grass", name: "Grass", working: "fibre", hardness: 1, colour: "#6f9a45" },
    },
    forms: {
        stick: { name: "Stick", shape: "long" },
        log: { name: "Log", shape: "round" },
        // a hard stone in the hand is a hammer: it strikes what's softer than flint off flint
        stone: { name: "Stone", shape: "lump", gives: ["strike"], hard: 5.5 },
        stalk: { name: "Stalks", shape: "strand", gives: ["bind"] },
        haft: { name: "Haft", shape: "long" },
        stake: { name: "Stake", shape: "long" },
        peg: { name: "Peg", shape: "peg" },
        handle: { name: "Handle", shape: "long" },
        blade: { name: "Blade", shape: "wedge", gives: ["cut"] },
        head: { name: "Axe head", shape: "wedge" },
        flakes: { name: "Flakes", shape: "chips" },
        cord: { name: "Cord", shape: "strand", gives: ["bind"] },
        post: { name: "Post", shape: "long" },
        axe: { name: "Axe", shape: "axe", gives: ["chop", "cut"] },
        knife: { name: "Knife", shape: "knife", gives: ["cut"] },
    },
    processes: [
        { id: "snap", verb: "Snap", takes: ["stick"], works: "fibrous",
            makes: [{ form: "haft", keeps: 0.8, seconds: 40 }, { form: "stake", keeps: 0.9, seconds: 20 }, { form: "peg", keeps: 0.2, seconds: 10 }] },
        { id: "carve", verb: "Carve", takes: ["stick"], works: "fibrous", needs: "cut",
            makes: [{ form: "handle", keeps: 0.6, seconds: 90 }] },
        { id: "knap", verb: "Knap", takes: ["stone"], works: "brittle", needs: "strike",
            makes: [{ form: "blade", keeps: 0.3, seconds: 60 }, { form: "head", keeps: 0.5, seconds: 120 }, { form: "flakes", keeps: 0.9, seconds: 15 }] },
        { id: "twist", verb: "Twist", takes: ["stalk"], works: "fibre",
            makes: [{ form: "cord", keeps: 0.5, seconds: 120 }] },
        { id: "split", verb: "Split", takes: ["log"], works: "fibrous", needs: "chop",
            makes: [{ form: "post", keeps: 0.9, seconds: 300 }, { form: "stake", keeps: 0.9, seconds: 120 }] },
        { id: "lash-axe", verb: "Lash", takes: ["head", "haft"], works: "brittle", needs: "bind",
            makes: [{ form: "axe", keeps: 1, seconds: 60 }] },
        { id: "haft-knife", verb: "Haft", takes: ["blade", "handle"], works: "brittle", needs: "bind",
            makes: [{ form: "knife", keeps: 1, seconds: 40 }] },
    ],
    items: {
        "core:log": { form: "log", material: "oak" },
        "core:birch_log": { form: "log", material: "birch" },
        "core:spruce_log": { form: "log", material: "spruce" },
        "core:dead_bush": { form: "stick", material: "deadwood" },
        "core:ore_flint": { form: "stone", material: "flint" },
        "materials:flint": { form: "stone", material: "flint" },
        "core:granite": { form: "stone", material: "granite" },
        "core:stone": { form: "stone", material: "granite" },
        "core:reed": { form: "stalk", material: "reed" },
        "core:cattail": { form: "stalk", material: "reed" },
        "core:grass_tuft": { form: "stalk", material: "grass" },
    },
}

/** What to hold for a capability, drawn as the badge on a dim offer. */
export const HOLD: Record<Capability, { form: string; material: string }> = {
    strike: { form: "stone", material: "granite" },
    cut: { form: "blade", material: "flint" },
    chop: { form: "axe", material: "flint" },
    bind: { form: "cord", material: "reed" },
}
