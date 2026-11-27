<script setup lang="ts">
import { computed, onActivated, ref } from "vue"
import type { LocalServer, PublicServer } from "../bridge/index.ts"
import { useCache } from "../cache/index.ts"
import { useStore } from "../game/store.ts"
import { Cover, Dropdown, FacetList, ICONS, KeptNote, ListEmpty, Notice, ResultsHead, RowSlot, SideCard, SideNav, SideStats, Toggle } from "../kit/index.ts"
import { format } from "../menu/library/format.ts"
import type { ActionsHandle } from "./actions.ts"
import FriendWorld from "./FriendWorld.vue"
import { places } from "./places.ts"

/**
 * Every place to join, laid out as the library's Browse: a side card that stays put (which list,
 * the numbers, and for public servers region, what it plays as, room and version) beside the
 * list. A public server opens its own page; one on this machine or a friend's world joins.
 * The query comes from the screen's search.
 */
type List = "public" | "favorites" | "local" | "friends"
const props = defineProps<{ actions: ActionsHandle; query: string }>()
const emit = defineEmits<{ server: [server: PublicServer]; player: [handle: string] }>()
const store = useStore()
const directory = useCache().cached("servers.list")
const list = ref<List>("public")
const regions = ref<string[]>([])
const tags = ref<string[]>([])
const room = ref(false)
const compatible = ref(true)
const sort = ref<"players" | "ping" | "name">("players")
const all = computed(() => directory.data.value?.servers ?? [])
const local = computed(() => directory.data.value?.local ?? [])
const friends = computed(() => places.playing(store.social))
const inMenu = computed(() => store.social?.me.state === "menu")
const facet = (pick: (s: PublicServer) => string[]) => [...new Set(all.value.flatMap(pick))].sort()
    .map((id) => ({ id: id, label: id, count: all.value.filter((s) => pick(s).includes(id)).length }))
const SORTS = [{ value: "players", label: "Most players" }, { value: "ping", label: "Lowest ping" }, { value: "name", label: "Name" }] as const
const SORT = { players: (a: PublicServer, b: PublicServer) => b.players - a.players, ping: (a: PublicServer, b: PublicServer) => (a.pingMs ?? 1e9) - (b.pingMs ?? 1e9), name: (a: PublicServer, b: PublicServer) => a.name.localeCompare(b.name) }
const matches = (text: string) => text.toLowerCase().includes(props.query.trim().toLowerCase())
const shown = computed(() => all.value.filter((s) =>
    matches(s.name + " " + s.description + " " + s.host)
    && (list.value === "public" || s.favorite)
    && (!regions.value.length || regions.value.includes(s.region))
    && tags.value.every((t) => s.tags.includes(t))
    && (!room.value || s.players < s.capacity)
    && (!compatible.value || s.compatible)).sort(SORT[sort.value]))
const shownLocal = computed(() => local.value.filter((s) => matches(s.title + " " + s.ref + " " + (s.owner ?? ""))))
const shownFriends = computed(() => friends.value.filter((f) => matches((f.presence?.world ?? "") + " " + f.username + " " + (f.name ?? ""))))

const lists = computed(() => [
    { id: "public" as const, label: "Public servers", icon: ICONS.servers, count: all.value.length },
    { id: "favorites" as const, label: "Favourites", icon: ICONS.heart, count: all.value.filter((s) => s.favorite).length },
    { id: "local" as const, label: "This machine", icon: ICONS.play, count: local.value.length },
    { id: "friends" as const, label: "Friends' worlds", icon: ICONS.players, count: friends.value.length },
])
const stats = computed(() => [
    { icon: ICONS.servers, value: format.count(all.value.length), label: "servers" },
    { icon: ICONS.players, value: format.count(all.value.reduce((n, s) => n + s.players, 0)), label: "playing" },
    { icon: ICONS.region, value: String(new Set(all.value.map((s) => s.region)).size), label: "regions" },
])
const TITLES: Record<List, string> = { public: "Servers", favorites: "Favourites", local: "This machine", friends: "Friends' worlds" }
const count = computed(() => !directory.data.value ? null
    : list.value === "local" ? `${shownLocal.value.length} running`
    : list.value === "friends" ? `${shownFriends.value.length} in a world`
    : `${shown.value.length} of ${all.value.length} servers`)
const joinLocal = (s: LocalServer) => props.actions.joinServer({ id: s.ref, ref: s.ref })
// kept alive by the screen: shown again from the cache, refreshed behind it
onActivated(() => directory.run({}))
</script>

<template>
    <div class="browse">
        <SideCard class="side">
            <SideNav v-model="list" :items="lists" />
            <SideStats :items="stats" />
            <template v-if="list === 'public' || list === 'favorites'">
                <FacetList v-model="regions" title="Region" :options="facet((s) => [s.region])" />
                <FacetList v-model="tags" title="Plays as" :options="facet((s) => s.tags)" />
                <section class="switches">
                    <Toggle v-model="room" label="Has room" />
                    <Toggle v-model="compatible" label="My version only" />
                </section>
            </template>
        </SideCard>

        <div class="results">
            <ResultsHead :title="query.trim() ? `“${query.trim()}”` : TITLES[list]" :count="count">
                <Dropdown v-if="list === 'public' || list === 'favorites'" v-model="sort" label="Sort" :options="SORTS" />
            </ResultsHead>
            <Notice v-if="actions.problem.value" tone="danger">{{ actions.problem.value }}</Notice>
            <KeptNote v-if="directory.problem.value && directory.at.value !== null" :at="directory.at.value" :problem="directory.problem.value" />

            <template v-if="list === 'local'">
                <RowSlot v-if="!directory.data.value" />
                <ListEmpty v-else-if="!shownLocal.length" title="Nothing running here"><p>Servers you run with <code>terra serve</code> or <code>terra dev</code> show up here.</p></ListEmpty>
                <div v-else class="rows">
                    <button v-for="s in shownLocal" :key="s.ref" type="button" class="row head" @click="joinLocal(s)">
                        <Cover :seed="s.folder" class="thumb" :width="24" />
                        <span class="name"><strong><i class="dot live" />{{ s.title }}</strong><small>{{ s.ref }} · {{ s.owner ? `run by @${s.owner}` : "run signed out" }} · {{ s.mods.length }} mods</small></span>
                        <span class="players">{{ actions.joining.value === s.ref ? "Joining…" : "Join »" }}</span>
                        <span class="ping">:{{ s.port }}</span>
                    </button>
                </div>
            </template>

            <template v-else-if="list === 'friends'">
                <ListEmpty v-if="!shownFriends.length" title="No friends in a world"><p>When a friend is playing, their world shows up here.</p></ListEmpty>
                <div v-else class="grid">
                    <FriendWorld v-for="f in shownFriends" :key="f.id" :friend="f" :busy="actions.busy.value" :in-menu="inMenu"
                        :joining="actions.joining.value === f.id" @open="emit('player', f.username)" @join="actions.joinFriend(f)" />
                </div>
            </template>

            <template v-else>
                <Notice v-if="(directory.problem.value && !directory.data.value) || directory.data.value?.status === 'unavailable'" tone="warn">
                    Public servers aren't listed yet: the directory isn't connected. Servers on this machine and friends' worlds are in the side list.</Notice>
                <RowSlot v-else-if="!directory.data.value" />
                <ListEmpty v-else-if="!shown.length" title="No servers match"><p>Loosen a filter to see more.</p></ListEmpty>
                <div v-else class="rows">
                    <button v-for="s in shown" :key="s.id" type="button" class="row head" @click="emit('server', s)">
                        <Cover :seed="s.id" class="thumb" :width="24" />
                        <span class="name"><strong><i class="dot" :class="{ live: s.players > 0 }" />{{ s.favorite ? "★ " : "" }}{{ s.name }}</strong><small>{{ s.host }} · {{ s.region }} · {{ s.tags.join(" · ") }}</small></span>
                        <span class="players"><span class="fill"><span :style="{ width: `${(s.players / s.capacity) * 100}%` }" /></span>{{ s.players }}/{{ s.capacity }}</span>
                        <span class="ping">{{ s.pingMs === null ? "—" : `${s.pingMs} ms` }}</span>
                    </button>
                </div>
            </template>
        </div>
    </div>
</template>

<style scoped>
.browse { display: grid; grid-template-columns: 17rem minmax(0, 1fr); gap: 3rem; align-items: start; }
.side { position: sticky; top: 2.5rem; }
.switches { display: grid; gap: 0.4rem; }
.rows { display: grid; gap: 0.75rem; }
.grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 1rem; }
.row { border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); transition: border-color 0.1s; }
.row:hover { border-color: var(--edge-lit); }
.head { display: grid; grid-template-columns: 7rem minmax(0, 1fr) 8rem 4rem; gap: 1.2rem; align-items: center; width: 100%; padding: 0 1.2rem 0 0; color: var(--ink); text-align: left; }
.thumb { height: 4rem; border-right: 1px solid var(--glass-edge); }
.name { display: grid; gap: 0.3rem; min-width: 0; }
strong { overflow: hidden; font: 400 0.8rem var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.row:hover strong { color: var(--accent-hi); }
small { overflow: hidden; color: var(--ink-faint); font-size: 0.68rem; text-overflow: ellipsis; white-space: nowrap; }
/* green: it's alive */
.dot { display: inline-block; width: 0.45rem; height: 0.45rem; margin-right: 0.7rem; background: var(--night-4); vertical-align: middle; }
.dot.live { background: var(--alive); }
.players { color: var(--ink-dim); font-size: 0.72rem; text-align: right; white-space: nowrap; }
.fill { display: inline-block; width: 3.5rem; height: 0.4rem; margin-right: 0.6rem; background: var(--night-3); vertical-align: middle; }
.fill span { display: block; height: 100%; background: var(--accent-hi); }
.ping { color: var(--ink-faint); font-size: 0.72rem; text-align: right; white-space: nowrap; }
@media (max-width: 900px) { .browse { grid-template-columns: minmax(0, 1fr); } .side { position: static; } .grid { grid-template-columns: minmax(0, 1fr); } }
</style>
