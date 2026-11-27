<script setup lang="ts">
import { computed, onActivated, ref, watch } from "vue"
import type { LocalMod, RegistrySearch, RegistryType } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { CardSlot, Dropdown, FacetList, ICONS, KeptNote, LayoutToggle, ListEmpty, Notice, Pager, ResultsHead, SideCard, SideNav, Spinner, Toggle } from "../../kit/index.ts"
import { CATEGORY_ICONS } from "./icons.ts"
import ModCard from "./ModCard.vue"
import { SORTS, search } from "./search.ts"

/**
 * One kind of the registry browsed, as the website's (pages/registry/browse.vue): a side card that
 * stays put (the registry's or your own, its categories, installed only) beside the results. `type`:
 * mods, or shaders and textures. "Yours" (mods) is what you're making, your profile's mods/. The
 * query comes from the screen's search.
 */
const props = defineProps<{ type: RegistryType; start: Partial<RegistrySearch>; query: string }>()
const emit = defineEmits<{ open: [name: string]; openLocal: [mod: LocalMod] }>()
type Kind = "all" | "yours"
const cache = useCache()
const front = cache.cached("registry.front")
const results = cache.cached("registry.search")
const local = cache.cached("mods.local")
const current = ref<RegistrySearch>(search.from({ ...props.start, query: props.query, type: props.type }))
const yoursOn = ref(false)
const layout = ref<"grid" | "list">("grid")
const categories = computed(() => front.data.value?.categories ?? [])
const labelOf = (id: string | undefined) => categories.value.find((c) => c.id === id)?.label ?? null
const kind = computed<Kind>({
    get: () => yoursOn.value ? "yours" : "all",
    set: (k) => {
        yoursOn.value = k === "yours"
        if (k === "yours") void local.run({})
    },
})
const kinds = computed(() => props.type === "mod" ? [
    { id: "all" as const, label: "All mods", icon: ICONS.mods },
    { id: "yours" as const, label: "Yours", icon: ICONS.profile, count: local.data.value?.length ?? null },
] : [
    { id: "all" as const, label: "Shaders & textures", icon: ICONS.textures },
])
const facets = computed(() => categories.value.map((c) => ({ ...c, icon: CATEGORY_ICONS[c.id] ?? CATEGORY_ICONS.other })))
const yours = computed(() => {
    const q = props.query.trim().toLowerCase()
    return (local.data.value ?? []).filter((m) => !q || `${m.name} ${m.title ?? ""} ${m.description}`.toLowerCase().includes(q))
})
const page = computed({ get: () => current.value.page, set: (value: number) => { current.value = { ...current.value, page: value } } })
const pages = computed(() => results.data.value ? Math.max(1, Math.ceil(results.data.value.total / results.data.value.pageSize)) : 1)
const title = computed(() => props.query.trim() ? `“${props.query.trim()}”` : yoursOn.value ? "Your mods" : props.type === "mod" ? "Mods" : "Shaders")
const count = computed(() => {
    const n = yoursOn.value ? (local.data.value ? yours.value.length : null) : results.data.value?.total ?? null
    return n === null ? null : `${n} ${n === 1 ? "result" : "results"}`
})
const range = computed(() => {
    const r = results.data.value
    if (!r) return ""
    const from = (r.page - 1) * r.pageSize + 1
    return `Showing ${from}–${from + r.listings.length - 1} of ${r.total}`
})

/** A first page's worth of empty cards, while a search nobody kept is first read. */
const SLOTS = 8

function set(change: Partial<RegistrySearch>) {
    current.value = { ...current.value, page: 1, ...change }
}
watch(() => props.start, (start) => { yoursOn.value = false; current.value = search.from({ ...start, query: props.query, type: props.type }) })
// typing settles for a moment before it searches
let timer: ReturnType<typeof setTimeout> | undefined
watch(() => props.query, (q) => { clearTimeout(timer); timer = setTimeout(() => set({ query: q.trim() }), 250) })
watch(current, (s) => results.run(s), { deep: true })
// kept alive by the library: shown again from the cache, refreshed behind it
onActivated(() => { void front.run({}); void results.run(current.value); void local.run({}) })
</script>

<template>
    <div class="browse">
        <SideCard class="side">
            <SideNav v-model="kind" :items="kinds" />
            <template v-if="!yoursOn">
                <FacetList v-if="type === 'mod'" title="Categories" :options="facets" :model-value="current.categories" @update:model-value="(v) => set({ categories: v })" />
                <section><Toggle label="Installed only" :model-value="current.installed" @update:model-value="(v) => set({ installed: v })" /></section>
            </template>
            <p v-else class="note">What you're making: your profile's mods folder, where <code>terra mod init</code> puts them.</p>
        </SideCard>

        <div class="results">
            <ResultsHead :title="title" :count="count">
                <!-- holds its place, so the controls beside it never shift as a refresh starts and ends -->
                <Spinner class="busy" :class="{ on: yoursOn ? local.busy.value : results.busy.value }" />
                <Dropdown v-if="!yoursOn" label="Sort" :options="SORTS" :model-value="current.sort" @update:model-value="(v) => set({ sort: v })" />
                <LayoutToggle v-model="layout" />
            </ResultsHead>

            <template v-if="yoursOn">
                <KeptNote v-if="local.problem.value && local.at.value !== null" :at="local.at.value" :problem="local.problem.value" />
                <Notice v-if="local.problem.value && !local.data.value" tone="danger">{{ local.problem.value }}</Notice>
                <ListEmpty v-else-if="local.data.value && !local.data.value.length" title="No mods of your own yet">
                    <p>Make one with <code>terra mod init &lt;name&gt;</code>: it lands in your profile's mods folder, and shows here.</p>
                </ListEmpty>
                <ListEmpty v-else-if="local.data.value && !yours.length" title="Nothing matches"><p>Try another word.</p></ListEmpty>
                <div v-else-if="local.data.value" class="cards" :class="layout">
                    <ModCard v-for="m in yours" :key="m.path" :listing="m" :row="layout === 'list'" yours @open="emit('openLocal', m)" />
                </div>
                <div v-else class="cards" :class="layout"><CardSlot v-for="n in SLOTS" :key="n" /></div>
            </template>
            <template v-else>
                <KeptNote v-if="results.problem.value && results.at.value !== null" :at="results.at.value" :problem="results.problem.value" />
                <Notice v-if="results.problem.value && !results.data.value" tone="danger">{{ results.problem.value }}</Notice>
                <ListEmpty v-else-if="results.data.value && !results.data.value.listings.length" title="Nothing matches that yet">
                    <p>Try fewer filters, or another word.</p>
                </ListEmpty>
                <template v-else-if="results.data.value">
                    <div class="cards" :class="layout">
                        <ModCard v-for="l in results.data.value.listings" :key="l.name" :listing="l" :row="layout === 'list'"
                            :tag="labelOf(l.categories[0])" @open="emit('open', l.name)" />
                    </div>
                    <footer><span>{{ range }}</span><Pager v-if="pages > 1" v-model="page" :pages="pages" /></footer>
                </template>
                <div v-else class="cards" :class="layout"><CardSlot v-for="n in SLOTS" :key="n" /></div>
            </template>
        </div>
    </div>
</template>

<style scoped>
.browse { display: grid; grid-template-columns: 17rem minmax(0, 1fr); gap: 3rem; align-items: start; }
/* stays put while the results scroll, at the page's own top */
.side { position: sticky; top: 2.5rem; }
.busy { visibility: hidden; }
.busy.on { visibility: visible; }
.note { color: var(--ink-dim); font: 0.82rem/1.5 var(--font-sans); }
.cards.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(15rem, 1fr)); gap: 1rem; }
.cards.list { display: grid; gap: 0.75rem; }
footer { display: flex; justify-content: space-between; align-items: center; margin-top: 2rem; color: var(--ink-faint); font-size: 0.72rem; }
@media (max-width: 900px) { .browse { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
