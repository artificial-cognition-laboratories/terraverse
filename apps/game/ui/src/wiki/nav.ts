// Where the wiki is: which section, and what's chosen in each. Sections
// link to each other (a reaction's reactants, a substance's reactions)
// through go().

import { inject, provide, reactive, ref, shallowRef, type InjectionKey, type ShallowRef } from "vue"
import type { MatterElement, MatterReaction, MatterRock, MatterSubstance } from "../bridge"

export type Section = "matter" | "reactions" | "heat" | "movement" | "vehicles" | "fluids" | "air" | "worlds" | "flora"

export type Table = {
    elements: MatterElement[]
    substances: MatterSubstance[]
    reactions: MatterReaction[]
    rocks: MatterRock[]
    bySymbol: Map<string, MatterElement>
    byId: Map<string, MatterSubstance>
}

export function WikiNav(opts: { place: "menu" | "game" }) {
    const state = reactive({
        section: "matter" as Section,
        chosen: { matter: "table", reactions: null, heat: null, movement: null, vehicles: null, fluids: null, air: null, worlds: null, flora: null } as Record<Section, string | null>,
    })
    const table: ShallowRef<Table | null> = shallowRef(null)
    const search = ref("")
    return {
        place: opts.place,
        /** Whether a reaction has been seen (always, in the menu). */
        state: state,
        table: table,
        search: search,
        go(section: Section, id?: string) {
            if (state.section !== section) search.value = ""
            state.section = section
            if (id !== undefined) state.chosen[section] = id
        },
    }
}

export type WikiNavHandle = ReturnType<typeof WikiNav>

const KEY: InjectionKey<WikiNavHandle> = Symbol("wiki")

export function provideWiki(nav: WikiNavHandle) {
    provide(KEY, nav)
}

export function useWiki() {
    const nav = inject(KEY)
    if (!nav) throw new Error("useWiki() outside the wiki")
    return nav
}

/** The loaded table; sections render only once it's there. */
export function useTable(): Table {
    const t = useWiki().table.value
    if (!t) throw new Error("the wiki's sections render only once matter.list has answered")
    return t
}
