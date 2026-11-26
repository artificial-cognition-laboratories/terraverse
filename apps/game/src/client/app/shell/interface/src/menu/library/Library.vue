<script setup lang="ts">
import { computed, KeepAlive, ref } from "vue"
import type { LocalMod, PublicServer, RegistrySearch } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { onEscape, Screen, SearchBox } from "../../kit/index.ts"
import Discover from "./Discover.vue"
import { RAIL, type Page } from "./icons.ts"
import ModPage from "./ModPage.vue"
import Mods from "./Mods.vue"
import Servers from "./Servers.vue"
import { search } from "./search.ts"

/**
 * The library, a full screen: Discover, Browse and Servers on its rail, one search along the top
 * (servers on Servers, mods everywhere else), and a mod's own page opened over the page it came from.
 */
/** `autofocus`: the search takes the keyboard on opening (the menu); in game it waits, so the key that opened it closes it. */
withDefaults(defineProps<{ autofocus?: boolean }>(), { autofocus: true })
const emit = defineEmits<{ close: [] }>()
const page = ref<Page>("discover")
const mod = ref<string | null>(null)
/** One of your own mods, opened: its page instead of a registry mod's. */
const local = ref<LocalMod | null>(null)
const modSearch = ref<Partial<RegistrySearch>>({})
const server = ref<string | null>(null)
const query = ref("")
// Every tab warm before it's clicked: what each one asks for first, fetched as the library opens
// (skipped for what this session already has live).
const cache = useCache()
cache.prefetch("registry.front", {})
cache.prefetch("registry.search", search.from({ query: "" }))
cache.prefetch("servers.list", {})
cache.prefetch("mods.local", {})
const pageLabel = computed(() => RAIL.find((item) => item.id === page.value)?.label ?? "")
const section = computed(() => local.value?.name ?? mod.value ?? pageLabel.value)

function browse(search: Partial<RegistrySearch>) {
    modSearch.value = search
    query.value = search.query ?? ""
    mod.value = null
    page.value = "mods"
}
function servers(chosen: PublicServer | null) {
    server.value = chosen?.id ?? null
    mod.value = null
    page.value = "servers"
}
function go(next: Page) {
    mod.value = null
    local.value = null
    query.value = ""
    page.value = next
}
onEscape(() => emit("close"))
// typing on Discover, or on a mod's page, is a search of the mods
function typed(q: string) {
    query.value = q
    mod.value = null
    local.value = null
    if (page.value === "discover") page.value = "mods"
}
</script>

<template>
    <Screen :page="page" :pages="RAIL" label="Library" :section="section" @update:page="go" @back="emit('close')">
        <template #search>
            <SearchBox :model-value="query" :autofocus="autofocus" :label="page === 'servers' ? 'Search servers' : 'Search mods'"
                :placeholder="page === 'servers' ? 'Search servers, hosts…' : 'Search mods, texture packs, authors…'" @update:model-value="typed" />
        </template>
        <ModPage v-if="local" :name="local.name" :local="local" :back="pageLabel" @back="local = null" />
        <ModPage v-else-if="mod" :name="mod" :back="pageLabel" @back="mod = null" @open="(name) => (mod = name)" />
        <!-- kept alive: a tab flicked back to is shown as it was left, and refreshes behind it -->
        <KeepAlive v-else>
            <Discover v-if="page === 'discover'" @open="(name) => (mod = name)" @browse="browse" @servers="servers" />
            <Mods v-else-if="page === 'mods'" :start="modSearch" :query="query" @open="(name) => (mod = name)" @open-local="(m) => (local = m)" />
            <Servers v-else :start="server" :query="query" />
        </KeepAlive>
    </Screen>
</template>
