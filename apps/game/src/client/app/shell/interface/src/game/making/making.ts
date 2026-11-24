/**
 * Making, as the page reasons about it (.agents/planning/making-and-labour.md): everything held is a
 * form of a material. What's in the crafting grid is what's worked; what's on the cursor is how
 * (a stone strikes, a blade cuts, cord binds; nothing is bare hands); what it could become is
 * offered. Joining hand-sized parts (a head and a haft, bound) is the same: one more process.
 *
 * TEMP(2026-11-23): a feel test, worked out in the page from catalogue.ts. Nothing is made yet:
 * the engine owns making once the design holds, and these shapes are the proposal for its data.
 */

/** How a material takes work: which processes apply to it. */
export type Working = "brittle" | "granular" | "fibrous" | "fibre"

export type Material = {
    id: string
    name: string
    working: Working
    /** Mohs: what it cuts, and what cuts it */
    hardness: number
    colour: string
}

/** What holding something lets you do to what's in the grid. */
export type Capability = "strike" | "cut" | "chop" | "bind"

export type Process = {
    id: string
    verb: string
    /** The forms that must be in the grid, in any slots; the first gives the made thing its material */
    takes: readonly string[]
    /** How the first one's material must take work */
    works: Working
    /** What must be on the cursor; none: bare hands */
    needs?: Capability
    makes: readonly { form: string; keeps: number; seconds: number }[]
}

/** A thing held: a form of a material, so heavy. */
export type Thing = { form: string; material: string; mass: number }

export type Catalogue = {
    materials: Record<string, Material>
    /** A form's name, how it's drawn, and what holding one lets you do */
    forms: Record<string, { name: string; shape: string; gives?: readonly Capability[]; hard?: number }>
    processes: readonly Process[]
    /** The game's items as things (a log is oak, flint ore a flint stone) */
    items: Record<string, Omit<Thing, "mass">>
}

/** One thing that could be made: dim, with what to hold, where the cursor can't do it. */
export type Offer = { key: string; process: Process; thing: Thing; seconds: number; needs: Capability | null }

export const making = {
    /** What the grid's things could become, worked by what's on the cursor. */
    offers(cat: Catalogue, grid: readonly Thing[], holding: Thing | null): Offer[] {
        const can = making.gives(cat, holding)
        return cat.processes.flatMap((p) => {
            const inputs = making.match(p, grid)
            if (!inputs) return []
            const first = cat.materials[inputs[0]!.material]!
            if (first.working !== p.works) return []
            const mass = inputs.reduce((sum, t) => sum + t.mass, 0)
            return p.makes.map((m): Offer => ({
                key: `${p.id}:${m.form}`,
                process: p,
                thing: { form: m.form, material: first.id, mass: mass * m.keeps },
                // harder takes longer: a granite head would be a day's work, a flint one minutes
                seconds: Math.round(m.seconds * (0.5 + first.hardness / 6)),
                needs: p.needs && !can.includes(p.needs) ? p.needs : null,
            }))
        }).sort((a, b) => Number(a.needs !== null) - Number(b.needs !== null))
    },
    /** What holding a thing lets you do: its form's gifts, a striking stone only if it's hard enough. */
    gives(cat: Catalogue, holding: Thing | null): Capability[] {
        if (!holding) return []
        const form = cat.forms[holding.form]
        const hard = cat.materials[holding.material]?.hardness ?? 0
        return (form?.gives ?? []).filter((c) => c !== "strike" || hard >= (form?.hard ?? 0))
    },
    /** The grid's things in a process's order, when the grid holds exactly what it takes. */
    match(p: Process, grid: readonly Thing[]): Thing[] | null {
        if (grid.length !== p.takes.length) return null
        const left = [...grid]
        const inOrder: Thing[] = []
        for (const form of p.takes) {
            const at = left.findIndex((t) => t.form === form)
            if (at < 0) return null
            inOrder.push(left.splice(at, 1)[0]!)
        }
        return inOrder
    },
}

/** "2 min", "45 s", "3 h": a time as short as it reads. */
export function formatTime(seconds: number): string {
    if (seconds < 90) return `${seconds} s`
    if (seconds < 90 * 60) return `${Math.round(seconds / 60)} min`
    return `${Math.round(seconds / 360) / 10} h`
}
