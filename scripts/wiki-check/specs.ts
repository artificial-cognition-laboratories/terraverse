// Every demo the wiki can show, as the page builds it (ui/src/wiki/demos.ts),
// written out for the engine-side runner. Substances get their molar mass
// from their elements, as matter.list does.
import { readFileSync } from "node:fs"
import { LAB_DEMOS, reactionDemo } from "../../apps/game/ui/src/wiki/demos"
import { AIR_DEMOS } from "../../apps/game/ui/src/wiki/air"
import { FLUID_DEMOS } from "../../apps/game/ui/src/wiki/fluids"
import type { MatterReaction, MatterSubstance } from "../../apps/game/ui/src/bridge"

const root = new URL("../../assets/matter/", import.meta.url)
const read = (name: string) => JSON.parse(readFileSync(new URL(name, root), "utf8"))
const masses = new Map<string, number>(read("elements.json").elements.map((e: { symbol: string; mass: number }) => [e.symbol, e.mass]))
const substances = new Map<string, MatterSubstance>()
for (const s of read("substances.json").substances) {
    let molar = 0
    for (const [symbol, n] of Object.entries(s.composition as Record<string, number>)) {
        const m = masses.get(symbol)
        if (m === undefined) throw new Error(`${s.id}: no element ${symbol}`)
        molar += m * n
    }
    substances.set(s.id, { ...s, molar_mass: molar, boil: s.boil ?? null })
}
const out: { name: string; spec: unknown; expect: string[]; wet?: unknown; hottest?: unknown; left?: unknown; active?: unknown }[] = []
for (const r of read("reactions.json").reactions as MatterReaction[]) {
    const demo = reactionDemo(r, substances)
    if (demo.runnable) out.push({ name: `reaction ${r.id}`, spec: demo.spec, expect: Object.keys(r.products) })
}
for (const d of LAB_DEMOS) out.push({ name: `lab ${d.id}`, spec: d.spec, expect: [] })
for (const d of FLUID_DEMOS) out.push({ name: `fluid ${d.id}`, spec: d.spec, expect: [], wet: d.wet })
for (const d of AIR_DEMOS) if (d.spec) out.push({ name: `air ${d.id}`, spec: d.spec, expect: [], hottest: d.hottest, left: d.left, active: d.active })
console.log(JSON.stringify(out))
