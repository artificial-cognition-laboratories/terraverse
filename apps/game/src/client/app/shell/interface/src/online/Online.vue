<script setup lang="ts">
import { computed, KeepAlive, ref } from "vue"
import type { PublicServer } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useCache } from "../cache/index.ts"
import { useStore } from "../game/store.ts"
import { onEscape, Screen, SearchBox } from "../kit/index.ts"
import ModPage from "../menu/library/ModPage.vue"
import { Actions } from "./actions.ts"
import Browse from "./Browse.vue"
import Discover from "./Discover.vue"
import FriendsPage from "./FriendsPage.vue"
import { RAIL, type Page } from "./icons.ts"
import PlayerPage from "./PlayerPage.vue"
import ServerPage from "./ServerPage.vue"
import SignInPage from "./SignInPage.vue"

/**
 * Online, a full screen laid out as the library: Discover, Servers, Friends and You (your own player page) on its rail,
 * one search along the top (servers, or friends on Friends), and a server's, a player's or a mod's
 * own page opened over the page it came from. The account corner's button opens it, menu and game.
 */
/** `autofocus`: the search takes the keyboard on opening (the menu); in game it waits. `start`: the page it opens on. */
const props = withDefaults(defineProps<{ autofocus?: boolean; start?: Page }>(), { autofocus: true, start: "discover" })
const emit = defineEmits<{ close: [] }>()
const store = useStore()
const actions = Actions({ bridge: useBridge() })
const page = ref<Page>(props.start)
const server = ref<string | null>(null)
const player = ref<string | null>(null)
const mod = ref<string | null>(null)
const query = ref("")
const cache = useCache()
cache.prefetch("servers.list", {})
const directory = cache.cached("servers.list")
const me = computed(() => store.account?.status === "signedIn" ? store.account.username : null)
const pageLabel = computed(() => RAIL.find((item) => item.id === page.value)?.label ?? "")
const section = computed(() => mod.value ?? (player.value !== null ? `@${player.value}` : null)
    ?? (server.value && directory.data.value?.servers.find((s) => s.id === server.value)?.name) ?? pageLabel.value)
const searchable = computed(() => page.value === "friends" ? "friends" : "servers")

function clear() {
    server.value = null
    player.value = null
    mod.value = null
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
// a mod opened from a server or a player keeps them to come back to
function openMod(name: string) { mod.value = name }
function typed(q: string) {
    query.value = q
    clear()
    if (page.value !== "browse" && page.value !== "friends") page.value = "browse"
}
onEscape(() => emit("close"))
</script>

<template>
    <Screen :page="page" :pages="RAIL" label="Online" :section="section" @update:page="go" @back="emit('close')">
        <template #search>
            <SearchBox :model-value="query" :autofocus="autofocus" :label="`Search ${searchable}`"
                :placeholder="searchable === 'friends' ? 'Search friends…' : 'Search servers, hosts, friends\' worlds…'" @update:model-value="typed" />
        </template>
        <!-- everything here is about you and your friends: signed out, only the way in -->
        <SignInPage v-if="!me" :actions="actions" />
        <ModPage v-else-if="mod" :name="mod" :back="player !== null ? `@${player}` : server ? 'Server' : pageLabel" @back="mod = null" @open="openMod" />
        <ServerPage v-else-if="server" :id="server" :back="pageLabel" :actions="actions" @back="server = null" @mod="openMod" />
        <PlayerPage v-else-if="player !== null" :handle="player" :back="pageLabel" :actions="actions" @back="player = null" @mod="openMod" />
        <!-- kept alive: a page flicked back to is shown as it was left, and refreshes behind it -->
        <KeepAlive v-else>
            <Discover v-if="page === 'discover'" :actions="actions" @player="openPlayer" @server="openServer" @browse="go('browse')" @friends="go('friends')" />
            <Browse v-else-if="page === 'browse'" :actions="actions" :query="query" @server="openServer" @player="openPlayer" />
            <FriendsPage v-else-if="page === 'friends'" :actions="actions" :query="query" @player="openPlayer" />
            <PlayerPage v-else-if="me" :handle="me" :back="null" :actions="actions" @mod="openMod" />
        </KeepAlive>
    </Screen>
</template>

