<script setup lang="ts">
import { computed, onActivated } from "vue"
import type { PublicServer, RegistrySearch } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { CardSlot, CategoryTile, Copier, KeptNote, Notice, Shelf, Spinner } from "../../kit/index.ts"
import Featured from "./Featured.vue"
import { CATEGORIES, CATEGORY_ICONS } from "./icons.ts"
import LiveServers from "./LiveServers.vue"
import ModCard from "./ModCard.vue"

/** The front page, as the website's registry (pages/registry/index.vue), less its claim: featured, categories, shelves of mods, live servers. */
const emit = defineEmits<{ open: [name: string]; browse: [search: Partial<RegistrySearch>]; servers: [server: PublicServer | null] }>()
const cache = useCache()
const front = cache.cached("registry.front")
const directory = cache.cached("servers.list")
const icon = (id: string) => CATEGORY_ICONS[id] ?? CATEGORY_ICONS.other
const labelOf = (id: string | undefined) => front.data.value?.categories.find((c) => c.id === id)?.label ?? null
// the busiest first, as many as the website's table shows
const live = computed(() => directory.data.value?.status === "ready"
    ? [...directory.data.value.servers].sort((a, b) => b.players - a.players).slice(0, 5) : [])
const liveEmpty = computed(() => directory.problem.value && !directory.data.value ? `The server list can't be reached: ${directory.problem.value}`
    : !directory.data.value ? "Asking for the server list…"
    : directory.data.value.status === "unavailable" ? "The server list can't be reached right now." : "No public servers yet.")
// the registry's own categories once it keeps them; till then the website's, shown but shut
const categories = computed(() => {
    const own = front.data.value?.categories ?? []
    return own.length ? own.map((c) => ({ ...c, open: true }))
        : CATEGORIES.map((c) => ({ ...c, count: null, open: false }))
})
// The shelves the registry keeps (bridge/registry.cpp: front), drawn before it answers, or when it
// can't, or has little: each one four wide, filled out with empty slots
const SHELF_WIDTH = 4
const PLACEHOLDER_SHELVES = [{ id: "stars", title: "Most starred" }, { id: "recent", title: "Recently updated" }]
const shelves = computed(() => {
    const own = front.data.value?.shelves ?? []
    const drawn = own.length ? own : PLACEHOLDER_SHELVES.map((s) => ({ ...s, listings: [], search: null }))
    return drawn.map((s) => ({ ...s, listings: s.listings.slice(0, SHELF_WIDTH), slots: Math.max(0, SHELF_WIDTH - s.listings.length) }))
})
const waiting = computed(() => !front.data.value && !front.problem.value)
const publish = Copier()
const STATE = { idle: "copy", copied: "copied", failed: "can't copy" } as const
// kept alive by the library: shown again from the cache, refreshed behind it
onActivated(() => { void front.run({}); void directory.run({}) })
</script>

<template>
    <div class="discover">
        <KeptNote v-if="front.problem.value && front.at.value !== null" :at="front.at.value" :problem="front.problem.value" />
        <Notice v-else-if="front.problem.value" tone="danger">The registry couldn't be reached: {{ front.problem.value }}</Notice>
        <Featured v-if="front.data.value?.featured.length" :listings="front.data.value.featured" @open="(name) => emit('open', name)" />
        <div v-else class="featured-slot">
            <CardSlot wide />
            <p class="slot-note"><template v-if="waiting"><Spinner /> Reading the registry…</template>
                <template v-else>Mods published with <code>terra publish</code> are featured here.</template></p>
        </div>
        <Shelf title="Categories" class="categories">
            <CategoryTile v-for="c in categories" :key="c.id" :label="c.label" :count="c.count" :disabled="!c.open"
                :rows="icon(c.id).rows" :palette="icon(c.id).palette" @open="emit('browse', { categories: [c.id] })" />
        </Shelf>
        <p v-if="front.data.value && !front.data.value.categories.length" class="unwired">The registry doesn't sort by category yet.</p>
        <Shelf v-for="shelf in shelves" :key="shelf.id" :title="shelf.title" :more="shelf.search ? 'See all' : undefined"
            @more="shelf.search && emit('browse', shelf.search)">
            <ModCard v-for="l in shelf.listings" :key="l.name" :listing="l" :tag="labelOf(l.categories[0])" @open="emit('open', l.name)" />
            <CardSlot v-for="n in shelf.slots" :key="'slot' + n" />
        </Shelf>
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
/* the banner's place, said over it, until something's featured */
.featured-slot { position: relative; }
.slot-note { position: absolute; left: 2rem; bottom: 2rem; display: flex; gap: 0.6rem; align-items: center; margin: 0; color: var(--ink-dim); font: 0.85rem var(--font-sans); }
.categories :deep(.grid) { gap: 0.75rem; }
.unwired { margin: 0.75rem 0 0; color: var(--ink-faint); font-size: 0.72rem; }
/* the website's foot of the registry (pages/registry/index.vue: .publish) */
.publish { display: flex; align-items: center; gap: 1.5rem; margin-top: 4rem; padding: 1.75rem 2rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); }
.publish > div { flex: 1; min-width: 0; }
.publish h2 { margin: 0 0 0.4rem; font: 700 1.2rem var(--font-sans); color: var(--ink); }
.publish p { margin: 0; font: 0.92rem/1.5 var(--font-sans); color: var(--ink-dim); }
.command { padding: 0.7rem 1rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); color: var(--ink); font: 0.85rem var(--font); }
.command:hover { border-color: var(--edge-lit); }
.command .prompt { color: var(--accent-hi); }
.command .state { margin-left: 0.8rem; color: var(--ink-faint); }
</style>
