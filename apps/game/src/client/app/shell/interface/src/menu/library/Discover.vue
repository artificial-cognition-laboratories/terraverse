<script setup lang="ts">
import { computed, onMounted } from "vue"
import type { PublicServer, RegistryFront, RegistrySearch, ServerDirectory } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { CategoryTile, Copier, ListEmpty, Notice, Shelf, Spinner } from "../../kit/index.ts"
import Featured from "./Featured.vue"
import { CATEGORIES, CATEGORY_ICONS } from "./icons.ts"
import LiveServers from "./LiveServers.vue"
import ModCard from "./ModCard.vue"
import { useRegistry } from "./registry.ts"
import { Request } from "./request.ts"

/** The front page, as the website's registry (pages/registry/index.vue), less its claim: featured, categories, shelves of mods, live servers. */
const emit = defineEmits<{ open: [name: string]; browse: [search: Partial<RegistrySearch>]; servers: [server: PublicServer | null] }>()
const bridge = useBridge()
const registry = useRegistry()
const front = Request<void, RegistryFront>(() => registry.front())
const directory = Request<void, ServerDirectory>(() => bridge.send("servers.list", {}))
const icon = (id: string) => CATEGORY_ICONS[id] ?? CATEGORY_ICONS.other
const labelOf = (id: string | undefined) => front.data.value?.categories.find((c) => c.id === id)?.label ?? null
// the busiest first, as many as the website's table shows
const live = computed(() => directory.data.value?.status === "ready"
    ? [...directory.data.value.servers].sort((a, b) => b.players - a.players).slice(0, 5) : [])
const liveEmpty = computed(() => directory.problem.value ? `The server list can't be reached: ${directory.problem.value}`
    : !directory.data.value ? "Asking for the server list…"
    : directory.data.value.status === "unavailable" ? "The server list can't be reached right now." : "No public servers yet.")
// the registry's own categories once it keeps them; till then the website's, shown but shut
const categories = computed(() => {
    const own = front.data.value?.categories ?? []
    return own.length ? own.map((c) => ({ ...c, open: true }))
        : CATEGORIES.map((c) => ({ ...c, count: null, open: false }))
})
const publish = Copier()
const STATE = { idle: "copy", copied: "copied", failed: "can't copy" } as const
onMounted(() => { front.run(); directory.run() })
</script>

<template>
    <div class="discover">
        <Notice v-if="front.problem.value" tone="danger">The registry couldn't be reached: {{ front.problem.value }}</Notice>
        <div v-else-if="!front.data.value" class="loading"><Spinner /> Reading the registry…</div>
        <template v-else>
            <Featured :listings="front.data.value.featured" @open="(name) => emit('open', name)" />
            <ListEmpty v-if="!front.data.value.featured.length && !front.data.value.shelves.length" title="Nothing on the registry yet">
                <p>Mods published with <code>terra publish</code> appear here.</p>
            </ListEmpty>
            <Shelf title="Categories" class="categories">
                <CategoryTile v-for="c in categories" :key="c.id" :label="c.label" :count="c.count" :disabled="!c.open"
                    :rows="icon(c.id).rows" :palette="icon(c.id).palette" @open="emit('browse', { categories: [c.id] })" />
            </Shelf>
            <p v-if="!front.data.value.categories.length" class="unwired">The registry doesn't sort by category yet.</p>
            <Shelf v-for="shelf in front.data.value.shelves" :key="shelf.id" :title="shelf.title" more="See all" @more="emit('browse', shelf.search)">
                <ModCard v-for="l in shelf.listings.slice(0, 4)" :key="l.name" :listing="l" :tag="labelOf(l.categories[0])" @open="emit('open', l.name)" />
            </Shelf>
        </template>
        <Shelf title="Live servers" more="All servers" :cols="null" @more="emit('servers', null)">
            <LiveServers :servers="live" :empty="liveEmpty" @open="(s) => emit('servers', s)" />
        </Shelf>
        <section class="publish">
            <div>
                <h2>Made something?</h2>
                <p>Publish it from the folder it's in. It's typed, versioned and one install away for everyone.</p>
            </div>
            <button type="button" class="command" @click="publish.copy('terra publish')">
                <span class="prompt">$</span> terra publish <span class="state">{{ STATE[publish.state.value] }}</span>
            </button>
        </section>
    </div>
</template>

<style scoped>
.loading { display: flex; gap: 10px; align-items: center; color: var(--ink-dim); font-size: 0.8rem; }
.categories :deep(.grid) { gap: 0.75rem; }
.unwired { margin: 0.75rem 0 0; color: var(--ink-faint); font-size: 0.72rem; }
/* the website's foot of the registry (pages/registry/index.vue: .publish) */
.publish { display: flex; align-items: center; gap: 1.5rem; margin-top: 4rem; padding: 1.75rem 2rem; border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); }
.publish > div { flex: 1; min-width: 0; }
.publish h2 { margin: 0 0 0.4rem; font: 700 1.2rem var(--font-sans); color: var(--ink); }
.publish p { margin: 0; font: 0.92rem/1.5 var(--font-sans); color: var(--ink-dim); }
.command { padding: 0.7rem 1rem; border: 1px solid var(--glass-edge); background: rgb(4 7 11 / 0.6); color: var(--ink); font: 0.85rem var(--font); }
.command:hover { border-color: var(--edge-lit); }
.command .prompt { color: var(--accent-hi); }
.command .state { margin-left: 0.8rem; color: var(--ink-faint); }
</style>
