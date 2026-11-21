<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import type { RegistryFront, RegistryResults, RegistrySearch } from "../../bridge/index.ts"
import { AsideColumn, Dropdown, FacetList, FilterChips, ListEmpty, Notice, PageIntro, Pager, SearchBox, Segmented, Spinner, Toggle } from "../../kit/index.ts"
import ModCard from "./ModCard.vue"
import { Request } from "./request.ts"
import { useRegistry } from "./registry.ts"
import { KINDS, SORTS, search } from "./search.ts"

/** Browsing the registry: filters on the left, results on the right, the query always shown as chips. */
const props = defineProps<{ start: Partial<RegistrySearch> }>()
const emit = defineEmits<{ open: [name: string] }>()
const registry = useRegistry()
const front = Request<void, RegistryFront>(() => registry.front())
const results = Request<RegistrySearch, RegistryResults>((s) => registry.search(s))
const current = ref<RegistrySearch>(search.from(props.start))
const typed = ref(current.value.query)
const layout = ref<"grid" | "rows">("rows")
const LAYOUTS = [{ value: "grid", label: "▦" }, { value: "rows", label: "≡" }] as const
const page = computed({ get: () => current.value.page, set: (value: number) => { current.value = { ...current.value, page: value } } })
const categories = computed(() => front.data.value?.categories ?? [])
const chips = computed(() => search.chips(current.value, categories.value))
const pages = computed(() => results.data.value ? Math.max(1, Math.ceil(results.data.value.total / results.data.value.pageSize)) : 1)
const type = computed({
    get: () => current.value.type ?? "all",
    set: (value: "all" | "mod" | "textures") => set({ type: value === "all" ? null : value }),
})

function set(change: Partial<RegistrySearch>) {
    current.value = { ...current.value, page: 1, ...change }
}
function drop(id: string) {
    current.value = search.drop(current.value, id)
    typed.value = current.value.query
}
function clear() {
    current.value = search.blank()
    typed.value = ""
}
watch(() => props.start, (start) => { current.value = search.from(start); typed.value = current.value.query })
watch(current, (s) => results.run(s), { deep: true })
// typing settles for a moment before it searches
let timer: ReturnType<typeof setTimeout> | undefined
watch(typed, (q) => { clearTimeout(timer); timer = setTimeout(() => set({ query: q.trim() }), 250) })
onMounted(() => { front.run(); results.run(current.value) })
</script>

<template>
    <AsideColumn class="mods">
        <template #aside>
            <SearchBox v-model="typed" autofocus label="Search the registry" placeholder="Search mods, authors…" />
            <fieldset><legend>Type</legend><Segmented v-model="type" look="line" size="sm" :options="KINDS" /></fieldset>
            <FacetList title="Categories" :options="categories" :model-value="current.categories" @update:model-value="(v) => set({ categories: v })" />
            <Toggle label="Installed only" :model-value="current.installed" @update:model-value="(v) => set({ installed: v })" />
        </template>
        <PageIntro title="Mods" note="Rules, machines, matter and whole games, made by the community and by us. Add them to a world or a server." />
        <div class="toolbar">
            <Pager v-model="page" :pages="pages" />
            <span v-if="results.data.value" class="total">{{ results.data.value.total }} results</span>
            <Spinner v-if="results.busy.value" />
            <Dropdown class="sort" label="Sort by" :options="SORTS" :model-value="current.sort" @update:model-value="(v) => set({ sort: v })" />
            <Segmented v-model="layout" look="line" size="sm" :options="LAYOUTS" />
        </div>
        <FilterChips :chips="chips" @remove="drop" @clear="clear" />
        <Notice v-if="results.problem.value" tone="danger">{{ results.problem.value }}</Notice>
        <ListEmpty v-else-if="results.data.value && !results.data.value.listings.length" title="Nothing matches">
            <p>Try fewer filters, or another word.</p>
        </ListEmpty>
        <div v-else-if="results.data.value" class="cards" :class="layout">
            <ModCard v-for="l in results.data.value.listings" :key="l.name" :listing="l" :dense="layout === 'rows'" @open="emit('open', l.name)" />
        </div>
        <Pager v-if="pages > 1" v-model="page" :pages="pages" class="foot" />
    </AsideColumn>
</template>

<style scoped>
.toolbar { display: flex; align-items: center; gap: 12px; padding-top: 2px; }
.total { color: var(--ink-faint); font-size: 11px; }
.sort { margin-left: auto; }
.foot { justify-content: center; padding-top: 6px; }
fieldset { display: grid; gap: 6px; min-width: 0; margin: 0; padding: 0; border: 0; }
legend { margin-bottom: 6px; padding: 0; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
.cards { display: grid; gap: 12px; }
.cards.grid { grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); }
.cards.rows { grid-template-columns: 1fr; gap: 6px; }
/* rows: the same card laid on its side */
.cards.rows :deep(.card) { display: grid; grid-template-columns: 132px minmax(0, 1fr); grid-template-rows: auto auto auto; }
.cards.rows :deep(.art) { grid-row: span 3; height: 100%; aspect-ratio: auto; border-bottom: 0; border-right: 1px solid var(--stone-3); }
.cards.rows :deep(.mark) { margin-top: 0; }
.cards.rows :deep(.text) { padding-top: 8px; }
.cards.rows :deep(.description) { grid-column: 2; margin: 6px 10px 0; }
.cards.rows :deep(.meta) { grid-column: 2; padding: 8px 10px; }
</style>
