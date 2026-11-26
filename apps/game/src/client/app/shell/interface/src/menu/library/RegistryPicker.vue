<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { Button, KeptNote, ListEmpty, Monogram, Notice, SearchBox, Spinner } from "../../kit/index.ts"
import { format } from "./format.ts"
import { search } from "./search.ts"

/** Mods on the registry to pick from: a search, and a row each with its Add. `chosen` ones say so; `hidden` aren't listed. */
const props = withDefaults(defineProps<{ chosen: readonly string[]; hidden?: readonly string[]; busy?: boolean; autofocus?: boolean }>(),
    { hidden: () => [], busy: false, autofocus: false })
const emit = defineEmits<{ pick: [listing: RegistryListing] }>()
const results = useCache().cached("registry.search")
const ask = (query: string) => results.run(search.from({ query: query, type: "mod" }))
const query = ref("")
const found = computed(() => (results.data.value?.listings ?? []).filter((l) => !props.hidden.includes(l.name)))
const chosen = (l: RegistryListing) => props.chosen.includes(l.name)
let typing: ReturnType<typeof setTimeout> | undefined
watch(query, (q) => { clearTimeout(typing); typing = setTimeout(() => void ask(q.trim()), 250) })
onMounted(() => ask(""))
</script>

<template>
    <div class="picker">
        <SearchBox v-model="query" :autofocus="autofocus" label="Search mods" placeholder="Search mods…" />
        <Spinner v-if="results.busy.value" />
        <KeptNote v-if="results.problem.value && results.at.value !== null" :at="results.at.value" :problem="results.problem.value" />
        <Notice v-else-if="results.problem.value" tone="danger">{{ results.problem.value }}</Notice>
        <div v-for="l in found" :key="l.name" class="row">
            <Monogram :name="l.name" :image="l.icon" :size="30" />
            <span class="copy"><strong>{{ l.title ?? format.bare(l.name) }}</strong><small>{{ l.description ?? l.name }}</small></span>
            <Button size="sm" :variant="chosen(l) ? 'plain' : 'primary'" :disabled="busy" @click="emit('pick', l)">{{ chosen(l) ? "Added" : "Add" }}</Button>
        </div>
        <ListEmpty v-if="results.data.value && !found.length">{{ query.trim() ? `No mods match “${query.trim()}”.` : "Nothing else on the registry yet." }}</ListEmpty>
    </div>
</template>

<style scoped>
.picker { display: grid; gap: 8px; min-width: 0; }
.row { display: grid; grid-template-columns: 30px minmax(0, 1fr) auto; align-items: center; gap: 10px; padding: 6px 0; border-bottom: 1px solid var(--stone-3); }
.copy { display: grid; gap: 2px; min-width: 0; }
.copy strong { overflow: hidden; font: 11px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.copy small { overflow: hidden; color: var(--ink-faint); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
</style>
