<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import type { LocalMod, RegistryFront, RegistryResults, RegistrySearch } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Dropdown, FacetList, ICONS, LayoutToggle, ListEmpty, Notice, Pager, ResultsHead, SideCard, SideNav, Spinner, Toggle } from "../../kit/index.ts"
import { CATEGORY_ICONS } from "./icons.ts"
import ModCard from "./ModCard.vue"
import { useRegistry } from "./registry.ts"
import { Request } from "./request.ts"
import { SORTS, search } from "./search.ts"

/**
 * Browse, as the website's (pages/registry/browse.vue): a side card that stays put (what kind,
 * which categories, installed only) beside the results. "Yours" is the mods you're making, your
 * profile's mods/. The query comes from the screen's search.
 */
const props = defineProps<{ start: Partial<RegistrySearch>; query: string }>()
const emit = defineEmits<{ open: [name: string]; openLocal: [mod: LocalMod] }>()
type Kind = "all" | "mod" | "textures" | "yours"
const registry = useRegistry()
const bridge = useBridge()
const front = Request<void, RegistryFront>(() => registry.front())
const results = Request<RegistrySearch, RegistryResults>((s) => registry.search(s))
const local = Request<void, LocalMod[]>(() => bridge.send("mods.local", {}))
const current = ref<RegistrySearch>(search.from({ ...props.start, query: props.query }))
const yoursOn = ref(false)
const layout = ref<"grid" | "list">("grid")
const categories = computed(() => front.data.value?.categories ?? [])
const labelOf = (id: string | undefined) => categories.value.find((c) => c.id === id)?.label ?? null
const kind = computed<Kind>({
    get: () => yoursOn.value ? "yours" : current.value.type ?? "all",
    set: (k) => {
        yoursOn.value = k === "yours"
        if (k === "yours") void local.run()
        else set({ type: k === "all" ? null : k })
    },
})
const kinds = computed(() => [
    { id: "all" as const, label: "Everything", icon: ICONS.all },
    { id: "mod" as const, label: "Mods", icon: ICONS.mods },
    { id: "textures" as const, label: "Textures", icon: ICONS.textures },
    { id: "yours" as const, label: "Yours", icon: ICONS.profile, count: local.data.value?.length ?? null },
])
const facets = computed(() => categories.value.map((c) => ({ ...c, icon: CATEGORY_ICONS[c.id] ?? CATEGORY_ICONS.other })))
const yours = computed(() => {
    const q = props.query.trim().toLowerCase()
    return (local.data.value ?? []).filter((m) => !q || `${m.name} ${m.title ?? ""} ${m.description}`.toLowerCase().includes(q))
})
const page = computed({ get: () => current.value.page, set: (value: number) => { current.value = { ...current.value, page: value } } })
const pages = computed(() => results.data.value ? Math.max(1, Math.ceil(results.data.value.total / results.data.value.pageSize)) : 1)
const title = computed(() => props.query.trim() ? `“${props.query.trim()}”` : yoursOn.value ? "Your mods" : "Browse")
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

function set(change: Partial<RegistrySearch>) {
    current.value = { ...current.value, page: 1, ...change }
}
watch(() => props.start, (start) => { yoursOn.value = false; current.value = search.from({ ...start, query: props.query }) })
// typing settles for a moment before it searches
let timer: ReturnType<typeof setTimeout> | undefined
watch(() => props.query, (q) => { clearTimeout(timer); timer = setTimeout(() => set({ query: q.trim() }), 250) })
watch(current, (s) => results.run(s), { deep: true })
onMounted(() => { front.run(); results.run(current.value); local.run() })
</script>

<template>
    <div class="browse">
        <SideCard class="side">
            <SideNav v-model="kind" :items="kinds" />
            <template v-if="!yoursOn">
                <FacetList title="Categories" :options="facets" :model-value="current.categories" @update:model-value="(v) => set({ categories: v })" />
                <section><Toggle label="Installed only" :model-value="current.installed" @update:model-value="(v) => set({ installed: v })" /></section>
            </template>
            <p v-else class="note">What you're making: your profile's mods folder, where <code>terra mod init</code> puts them.</p>
        </SideCard>

        <div class="results">
            <ResultsHead :title="title" :count="count">
                <Spinner v-if="yoursOn ? local.busy.value : results.busy.value" />
                <Dropdown v-if="!yoursOn" label="Sort" :options="SORTS" :model-value="current.sort" @update:model-value="(v) => set({ sort: v })" />
                <LayoutToggle v-model="layout" />
            </ResultsHead>

            <template v-if="yoursOn">
                <Notice v-if="local.problem.value" tone="danger">{{ local.problem.value }}</Notice>
                <ListEmpty v-else-if="local.data.value && !local.data.value.length" title="No mods of your own yet">
                    <p>Make one with <code>terra mod init &lt;name&gt;</code>: it lands in your profile's mods folder, and shows here.</p>
                </ListEmpty>
                <ListEmpty v-else-if="local.data.value && !yours.length" title="Nothing matches"><p>Try another word.</p></ListEmpty>
                <div v-else-if="local.data.value" class="cards" :class="layout">
                    <ModCard v-for="m in yours" :key="m.path" :listing="m" :row="layout === 'list'" yours @open="emit('openLocal', m)" />
                </div>
            </template>
            <template v-else>
                <Notice v-if="results.problem.value" tone="danger">{{ results.problem.value }}</Notice>
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
            </template>
        </div>
    </div>
</template>

<style scoped>
.browse { display: grid; grid-template-columns: 17rem minmax(0, 1fr); gap: 3rem; align-items: start; }
/* stays put while the results scroll, at the page's own top */
.side { position: sticky; top: 2.5rem; }
.note { color: var(--ink-dim); font: 0.82rem/1.5 var(--font-sans); }
.cards.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(15rem, 1fr)); gap: 1rem; }
.cards.list { display: grid; gap: 0.75rem; }
footer { display: flex; justify-content: space-between; align-items: center; margin-top: 2rem; color: var(--ink-faint); font-size: 0.72rem; }
@media (max-width: 900px) { .browse { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
