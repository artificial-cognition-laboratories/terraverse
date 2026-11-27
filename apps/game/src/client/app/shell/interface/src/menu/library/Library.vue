<script setup lang="ts">
import { computed, KeepAlive, ref, watch } from "vue"
import type { LocalMod, PublicServer, RegistrySearch } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { useCache } from "../../cache/index.ts"
import { useStore } from "../../game/store.ts"
import { ICONS, ListEmpty, onEscape, Screen, SearchBox } from "../../kit/index.ts"
import { Actions } from "../../online/actions.ts"
import Browse from "../../online/Browse.vue"
import PlayerPage from "../../online/PlayerPage.vue"
import Account from "./account/Account.vue"
import ServerPage from "../../online/ServerPage.vue"
import Discover from "./Discover.vue"
import { RAIL, SEARCHES, type LibraryOpen, type Page } from "./icons.ts"
import ModPage from "./ModPage.vue"
import Mods from "./Mods.vue"
import { search } from "./search.ts"
import Upcoming from "./Upcoming.vue"

/**
 * The library, a full screen: everything to get or go to, a kind to each page of its rail
 * (Discover mixes them; Public is servers to join; Packs, Mods, Worlds and Shaders are things to
 * install), and your account at its foot. One search along the top, of the page you're on; a
 * mod's, a server's or a player's own page opened over the page it came from.
 */
/**
 * `autofocus`: the search takes the keyboard on opening (the menu); in game it waits, so the key that
 * opened it closes it. `open`: where to open, asked from outside
 * (the friends popover: a friend's page, or your account).
 */
const props = withDefaults(defineProps<{ autofocus?: boolean; open?: LibraryOpen | null }>(), { autofocus: true, open: null })
const emit = defineEmits<{ close: [] }>()
const store = useStore()
const actions = Actions({ bridge: useBridge() })
const page = ref<Page>("discover")
const mod = ref<string | null>(null)
const server = ref<string | null>(null)
const player = ref<string | null>(null)
/** One of your own mods, opened: its page instead of a registry mod's. */
const local = ref<LocalMod | null>(null)
const modSearch = ref<Partial<RegistrySearch>>({})
const query = ref("")
// Every tab warm before it's clicked: what each one asks for first, fetched as the library opens
// (skipped for what this session already has live).
const cache = useCache()
cache.prefetch("registry.front", {})
cache.prefetch("registry.search", search.from({ query: "", type: "mod" }))
cache.prefetch("mods.local", {})
cache.prefetch("servers.list", {})
const directory = cache.cached("servers.list")
const me = computed(() => store.account?.status === "signedIn" ? store.account.username : null)
const pageLabel = computed(() => RAIL.find((item) => item.id === page.value)?.label ?? "")
const section = computed(() => local.value?.name ?? mod.value ?? (player.value !== null ? `@${player.value}` : null)
    ?? (server.value && directory.data.value?.servers.find((s) => s.id === server.value)?.name) ?? pageLabel.value)
const over = computed(() => player.value !== null ? `@${player.value}` : server.value ? "Server" : pageLabel.value)

function clear() {
    mod.value = null
    local.value = null
    server.value = null
    player.value = null
}
function browse(search: Partial<RegistrySearch>) {
    modSearch.value = search
    query.value = search.query ?? ""
    clear()
    page.value = search.type === "textures" ? "shaders" : "mods"
}
function go(next: Page) {
    clear()
    query.value = ""
    page.value = next
}
function openServer(s: PublicServer) {
    clear()
    server.value = s.id
}
function openPlayer(handle: string) {
    clear()
    player.value = handle
}
watch(() => props.open, (to) => {
    if (!to) return
    if ("player" in to) openPlayer(to.player)
    else go(to.page)
}, { immediate: true })
onEscape(() => emit("close"))
// typing on Discover or your page is a search of the mods, as nothing mixes kinds yet
function typed(q: string) {
    query.value = q
    clear()
    if (page.value === "discover" || page.value === "account") page.value = "mods"
}
</script>

<template>
    <Screen :page="page" :pages="RAIL" label="Library" :section="section" @update:page="go" @back="emit('close')">
        <template #search>
            <SearchBox :model-value="query" :autofocus="autofocus" :label="`Search ${pageLabel}`"
                :placeholder="SEARCHES[page]" @update:model-value="typed" />
        </template>
        <ModPage v-if="local" :name="local.name" :local="local" :back="pageLabel" @back="local = null" />
        <!-- a mod opened from a server or a player keeps them to come back to -->
        <ModPage v-else-if="mod" :name="mod" :back="over" @back="mod = null" @open="(name) => (mod = name)" />
        <ServerPage v-else-if="server" :id="server" :back="pageLabel" :actions="actions" @back="server = null" @mod="(name) => (mod = name)" />
        <PlayerPage v-else-if="player !== null" :handle="player" :back="pageLabel" :actions="actions" @back="player = null" @mod="(name) => (mod = name)" />
        <!-- kept alive: a tab flicked back to is shown as it was left, and refreshes behind it -->
        <KeepAlive v-else>
            <Discover v-if="page === 'discover'" @open="(name) => (mod = name)" @browse="browse" />
            <Browse v-else-if="page === 'public'" :actions="actions" :query="query" @server="openServer" @player="openPlayer" />
            <Upcoming v-else-if="page === 'packs'" key="packs" title="Packs" empty="No packs yet"
                :scopes="[{ id: 'all', label: 'All packs', icon: ICONS.blocks }, { id: 'installed', label: 'Installed', icon: ICONS.download }, { id: 'yours', label: 'Yours', icon: ICONS.profile }]"
                note="A pack is a game set up by someone else: its mods, rules, a solar system and where you start. Install one to play it privately.">
                <p>Packs aren't published yet: when they are, they're found here, beside the servers running them.</p>
            </Upcoming>
            <Mods v-else-if="page === 'mods'" key="mods" type="mod" :start="modSearch" :query="query" @open="(name) => (mod = name)" @open-local="(m) => (local = m)" />
            <Upcoming v-else-if="page === 'worlds'" key="worlds" title="Worlds" empty="No worlds yet"
                :scopes="[{ id: 'seeds', label: 'Seeds', icon: ICONS.region }, { id: 'worlds', label: 'Built worlds', icon: ICONS.blocks }, { id: 'yours', label: 'Yours', icon: ICONS.profile }]"
                note="A seed is a world code anyone can make; a built world is one with what's been made in it.">
                <p>Worlds aren't published yet: when they are, seeds and built worlds are found here.</p>
            </Upcoming>
            <Mods v-else-if="page === 'shaders'" key="shaders" type="textures" :start="modSearch" :query="query" @open="(name) => (mod = name)" @open-local="(m) => (local = m)" />
            <Account v-else-if="me" key="account" :handle="me" :actions="actions" @mod="(name) => (mod = name)" @page="openPlayer" />
            <ListEmpty v-else key="signed-out" title="Not signed in"><p>Sign in from the account corner to keep your account here.</p></ListEmpty>
        </KeepAlive>
    </Screen>
</template>
