<script setup lang="ts">
import { onMounted, ref } from "vue"
import type { PublicServer, RegistryFront, RegistrySearch, ServerDirectory } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { CategoryTile, ListEmpty, Notice, SearchBox, Shelf, Spinner } from "../../kit/index.ts"
import Featured from "./Featured.vue"
import { CATEGORY_ICONS } from "./icons.ts"
import ModCard from "./ModCard.vue"
import { Request } from "./request.ts"
import ServerCard from "./ServerCard.vue"
import { useRegistry } from "./registry.ts"

/** The front page: search, featured, categories, then shelves of mods and live servers. */
const emit = defineEmits<{ open: [name: string]; browse: [search: Partial<RegistrySearch>]; servers: [server: PublicServer | null] }>()
const bridge = useBridge()
const registry = useRegistry()
const front = Request<void, RegistryFront>(() => registry.front())
const servers = Request<void, ServerDirectory>(() => bridge.send("servers.list", {}))
const query = ref("")
const icon = (id: string) => CATEGORY_ICONS[id] ?? CATEGORY_ICONS.other
onMounted(() => { front.run(); servers.run() })
</script>

<template>
    <div class="discover">
        <form class="search" @submit.prevent="emit('browse', { query: query.trim() })">
            <SearchBox v-model="query" autofocus label="Search mods and texture packs" placeholder="Search mods, texture packs, authors…" />
        </form>
        <Notice v-if="front.problem.value" tone="danger">The registry couldn't be reached: {{ front.problem.value }}</Notice>
        <div v-else-if="!front.data.value" class="loading"><Spinner /> Reading the registry…</div>
        <template v-else>
            <Featured :listings="front.data.value.featured" @open="(name) => emit('open', name)" />
            <ListEmpty v-if="!front.data.value.featured.length && !front.data.value.shelves.length" title="Nothing on the registry yet">
                <p>Mods published with <code>terra publish</code> appear here.</p>
            </ListEmpty>
            <section v-if="front.data.value.categories.length" class="categories">
                <h3>Categories</h3>
                <div class="grid">
                    <CategoryTile v-for="c in front.data.value.categories" :key="c.id" :label="c.label" :count="c.count"
                        :rows="icon(c.id).rows" :palette="icon(c.id).palette" @open="emit('browse', { categories: [c.id] })" />
                </div>
            </section>
            <Shelf v-for="shelf in front.data.value.shelves" :key="shelf.id" :title="shelf.title" more="See all"
                @more="emit('browse', shelf.search)">
                <ModCard v-for="l in shelf.listings" :key="l.name" :listing="l" @open="emit('open', l.name)" />
            </Shelf>
        </template>
        <Shelf title="Live servers" more="All servers" :min="240" @more="emit('servers', null)">
            <template v-if="servers.data.value?.status === 'ready'">
                <ServerCard v-for="s in servers.data.value.servers.filter((s) => s.players > 0)" :key="s.id" :server="s" @open="emit('servers', s)" />
            </template>
            <ListEmpty v-else-if="servers.problem.value || servers.data.value">The server list isn't available right now.</ListEmpty>
        </Shelf>
    </div>
</template>

<style scoped>
.discover { display: grid; align-content: start; gap: 26px; max-width: 904px; margin: 0 auto; padding: 22px 26px 40px; }
.search :deep(.search-box) { height: 40px; }
.search :deep(input) { font-size: 12px; }
.loading { display: flex; gap: 10px; align-items: center; color: var(--ink-dim); font-size: 12px; }
.categories { display: grid; gap: 10px; }
h3 { margin: 0; padding-bottom: 6px; border-bottom: 1px solid var(--stone-3); font: 12px var(--font-display); text-transform: uppercase; }
.categories .grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 8px; }
</style>
