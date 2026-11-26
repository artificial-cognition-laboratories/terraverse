<script setup lang="ts">
import { computed, onActivated, ref } from "vue"
import type { PublicServer } from "../../bridge/index.ts"
import { BridgeError } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { useCache } from "../../cache/index.ts"
import { Button, Cover, Dropdown, FacetList, ICONS, KeptNote, ListEmpty, Notice, ResultsHead, RowSlot, SideCard, SideNav, SideStats, Toggle } from "../../kit/index.ts"
import { format } from "./format.ts"

/**
 * Public servers, laid out as the website's Browse: a side card that stays put (which list, the
 * numbers, region, what it plays as, room and version) beside the list, the chosen one opened
 * beneath its row. The query comes from the screen's search.
 */
const props = defineProps<{ start: string | null; query: string }>()
const bridge = useBridge()
const directory = useCache().cached("servers.list")
const regions = ref<string[]>([])
const tags = ref<string[]>([])
const room = ref(false)
const list = ref<"all" | "favorites">("all")
const compatible = ref(true)
const sort = ref<"players" | "ping" | "name">("players")
const open = ref<string | null>(props.start)
const problem = ref<string | null>(null)
const all = computed(() => directory.data.value?.servers ?? [])
const facet = (pick: (s: PublicServer) => string[]) => [...new Set(all.value.flatMap(pick))].sort()
    .map((id) => ({ id: id, label: id, count: all.value.filter((s) => pick(s).includes(id)).length }))
const SORTS = [{ value: "players", label: "Most players" }, { value: "ping", label: "Lowest ping" }, { value: "name", label: "Name" }] as const
const SORT = { players: (a: PublicServer, b: PublicServer) => b.players - a.players, ping: (a: PublicServer, b: PublicServer) => (a.pingMs ?? 1e9) - (b.pingMs ?? 1e9), name: (a: PublicServer, b: PublicServer) => a.name.localeCompare(b.name) }
const shown = computed(() => all.value.filter((s) =>
    (s.name + " " + s.description + " " + s.host).toLowerCase().includes(props.query.trim().toLowerCase())
    && (list.value === "all" || s.favorite)
    && (!regions.value.length || regions.value.includes(s.region))
    && tags.value.every((t) => s.tags.includes(t))
    && (!room.value || s.players < s.capacity)
    && (!compatible.value || s.compatible)).sort(SORT[sort.value]))

const lists = computed(() => [
    { id: "all" as const, label: "All servers", icon: ICONS.servers, count: all.value.length },
    { id: "favorites" as const, label: "Favourites", icon: ICONS.heart, count: all.value.filter((s) => s.favorite).length },
])
const stats = computed(() => [
    { icon: ICONS.servers, value: format.count(all.value.length), label: "servers" },
    { icon: ICONS.players, value: format.count(all.value.reduce((n, s) => n + s.players, 0)), label: "playing" },
    { icon: ICONS.region, value: String(new Set(all.value.map((s) => s.region)).size), label: "regions" },
])

async function act(fn: () => Promise<unknown>) {
    problem.value = null
    try { await fn() }
    catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
    }
}
const join = (s: PublicServer) => act(() => bridge.send("servers.join", { ref: s.ref }))
const favorite = (s: PublicServer) => act(async () => { await bridge.send("servers.favorite.set", { id: s.id, favorite: !s.favorite }); await directory.refresh({}) })
// kept alive by the library: shown again from the cache, refreshed behind it
onActivated(() => directory.run({}))
</script>

<template>
    <div class="browse">
        <SideCard class="side">
            <SideNav v-model="list" :items="lists" />
            <SideStats :items="stats" />
            <FacetList v-model="regions" title="Region" :options="facet((s) => [s.region])" />
            <FacetList v-model="tags" title="Plays as" :options="facet((s) => s.tags)" />
            <section class="switches">
                <Toggle v-model="room" label="Has room" />
                <Toggle v-model="compatible" label="My version only" />
            </section>
        </SideCard>

        <div class="results">
            <ResultsHead :title="query.trim() ? `“${query.trim()}”` : 'Servers'" :count="directory.data.value ? `${shown.length} of ${all.length} servers` : null">
                <Dropdown v-model="sort" label="Sort" :options="SORTS" />
            </ResultsHead>
            <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
            <KeptNote v-if="directory.problem.value && directory.at.value !== null" :at="directory.at.value" :problem="directory.problem.value" />
            <Notice v-if="(directory.problem.value && !directory.data.value) || directory.data.value?.status === 'unavailable'" tone="warn">The server list isn't available right now.</Notice>
            <RowSlot v-else-if="!directory.data.value" />
            <ListEmpty v-else-if="!shown.length" title="No servers match"><p>Loosen a filter to see more.</p></ListEmpty>
            <div v-else class="rows">
                <article v-for="s in shown" :key="s.id" class="row" :class="{ open: open === s.id }">
                    <button type="button" class="head" @click="open = open === s.id ? null : s.id">
                        <Cover :seed="s.id" class="thumb" :width="24" />
                        <span class="name"><strong><i class="dot" :class="{ live: s.players > 0 }" />{{ s.favorite ? "★ " : "" }}{{ s.name }}</strong><small>{{ s.host }} · {{ s.region }} · {{ s.tags.join(" · ") }}</small></span>
                        <span class="players"><span class="fill"><span :style="{ width: `${(s.players / s.capacity) * 100}%` }" /></span>{{ s.players }}/{{ s.capacity }}</span>
                        <span class="ping">{{ s.pingMs === null ? "—" : `${s.pingMs} ms` }}</span>
                    </button>
                    <div v-if="open === s.id" class="detail">
                        <p>{{ s.description }}</p>
                        <dl>
                            <div><dt>Version</dt><dd>{{ s.version }}<template v-if="!s.compatible"> (not this build)</template></dd></div>
                            <div><dt>Mods</dt><dd>{{ s.mods.join(", ") || "None" }}</dd></div>
                        </dl>
                        <div class="actions">
                            <Button size="sm" @click="favorite(s)">{{ s.favorite ? "Unfavourite" : "Favourite" }}</Button>
                            <Button variant="primary" size="sm" :disabled="!s.compatible || s.players >= s.capacity" @click="join(s)">Join »</Button>
                        </div>
                    </div>
                </article>
            </div>
        </div>
    </div>
</template>

<style scoped>
.browse { display: grid; grid-template-columns: 17rem minmax(0, 1fr); gap: 3rem; align-items: start; }
.side { position: sticky; top: 2.5rem; }
.switches { display: grid; gap: 0.4rem; }
.rows { display: grid; gap: 0.75rem; }
.row { border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); transition: border-color 0.1s; }
.row:hover, .row.open { border-color: var(--edge-lit); }
.head { display: grid; grid-template-columns: 7rem minmax(0, 1fr) 8rem 4rem; gap: 1.2rem; align-items: center; width: 100%; padding: 0 1.2rem 0 0; border: 0; background: none; color: var(--ink); text-align: left; }
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
.detail { display: grid; gap: 0.9rem; padding: 1rem 1.2rem 1.2rem 8.2rem; border-top: 1px solid var(--glass-edge); }
.detail p { margin: 0; color: var(--ink-dim); font: 0.88rem/1.55 var(--font-sans); }
dl { display: grid; gap: 0.3rem; margin: 0; font-size: 0.75rem; }
dl div { display: flex; gap: 0.75rem; }
dt { width: 5rem; color: var(--ink-faint); }
dd { margin: 0; color: var(--ink); }
.actions { display: flex; gap: 0.5rem; }
@media (max-width: 900px) { .browse { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
