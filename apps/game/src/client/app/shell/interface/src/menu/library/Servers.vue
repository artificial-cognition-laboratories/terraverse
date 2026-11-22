<script setup lang="ts">
import { computed, onMounted, ref } from "vue"
import type { PublicServer, ServerDirectory } from "../../bridge/index.ts"
import { BridgeError } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { AsideColumn, Button, Dropdown, Cover, FacetList, ListEmpty, Notice, PageIntro, SearchBox, Spinner, Toggle } from "../../kit/index.ts"
import { Request } from "./request.ts"

/** Public servers: filters on the left, the list on the right, the chosen one opened beneath its row. */
const props = defineProps<{ start: string | null }>()
const bridge = useBridge()
const directory = Request<void, ServerDirectory>(() => bridge.send("servers.list", {}))
const query = ref("")
const regions = ref<string[]>([])
const tags = ref<string[]>([])
const room = ref(false)
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
    (s.name + " " + s.description + " " + s.host).toLowerCase().includes(query.value.trim().toLowerCase())
    && (!regions.value.length || regions.value.includes(s.region))
    && tags.value.every((t) => s.tags.includes(t))
    && (!room.value || s.players < s.capacity)
    && (!compatible.value || s.compatible)).sort(SORT[sort.value]))

async function act(fn: () => Promise<unknown>) {
    problem.value = null
    try { await fn() }
    catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
    }
}
const join = (s: PublicServer) => act(() => bridge.send("servers.join", { id: s.id }))
const favorite = (s: PublicServer) => act(async () => { await bridge.send("servers.favorite.set", { id: s.id, favorite: !s.favorite }); await directory.run() })
onMounted(() => directory.run())
</script>

<template>
    <AsideColumn class="servers">
        <template #aside>
            <SearchBox v-model="query" autofocus label="Search servers" placeholder="Search servers, hosts…" />
            <Toggle v-model="room" label="Has room" />
            <Toggle v-model="compatible" label="My version only" />
            <FacetList v-model="regions" title="Region" :options="facet((s) => [s.region])" />
            <FacetList v-model="tags" title="Plays as" :options="facet((s) => s.tags)" />
        </template>
        <PageIntro title="Servers" note="Worlds other people host, open to join. Ping is from here; a server on another build can be seen but not joined." />
        <div class="toolbar">
            <span v-if="directory.data.value" class="total">{{ shown.length }} of {{ all.length }} servers</span>
            <Dropdown v-model="sort" class="sort" label="Sort by" :options="SORTS" />
        </div>
            <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
            <Notice v-if="directory.problem.value || directory.data.value?.status === 'unavailable'" tone="warn">The server list isn't available right now.</Notice>
            <div v-else-if="!directory.data.value"><Spinner /></div>
            <ListEmpty v-else-if="!shown.length" title="No servers match"><p>Loosen a filter to see more.</p></ListEmpty>
            <article v-for="s in shown" :key="s.id" class="row" :class="{ open: open === s.id }">
                <button type="button" class="head" @click="open = open === s.id ? null : s.id">
                    <Cover :seed="s.id" class="thumb" />
                    <span class="name"><strong>{{ s.favorite ? "★ " : "" }}{{ s.name }}</strong><small>{{ s.host }} · {{ s.region }} · {{ s.tags.join(" · ") }}</small></span>
                    <span class="players"><i :style="{ width: `${(s.players / s.capacity) * 100}%` }" />{{ s.players }}/{{ s.capacity }}</span>
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
                        <Button variant="primary" :disabled="!s.compatible || s.players >= s.capacity" @click="join(s)">Join »</Button>
                    </div>
                </div>
            </article>
    </AsideColumn>
</template>

<style scoped>
.toolbar { display: flex; align-items: center; gap: 12px; }
.total { color: var(--ink-faint); font-size: 11px; }
.sort { margin-left: auto; }
.servers :deep(main) { gap: 8px; }

.row { flex: none; border: 1px solid var(--stone-3); }
.row:hover { border-color: var(--stone-4); }
.row.open { border-color: var(--accent-lo); }
.head { display: grid; grid-template-columns: 64px minmax(0, 1fr) 90px 52px; gap: 14px; align-items: center; width: 100%; padding: 0 12px 0 0; border: 0; background: none; color: var(--ink); text-align: left; }

.thumb { height: 44px; border-right: 1px solid var(--stone-3); }
.name { display: grid; gap: 3px; min-width: 0; }
strong { overflow: hidden; font: 11px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
small { overflow: hidden; color: var(--ink-faint); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
.players { position: relative; padding: 3px 6px; background: var(--stone-0); box-shadow: var(--sunk); font-size: 10px; text-align: right; }
.players i { position: absolute; inset: 0 auto 0 0; background: rgb(74 155 100 / 0.35); }
.ping { color: var(--ink-dim); font-size: 10px; white-space: nowrap; }
.detail { display: grid; gap: 12px; padding: 12px 14px 14px 78px; border-top: 1px solid var(--stone-3); }
.detail p { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
dl { display: grid; gap: 4px; margin: 0; font-size: 11px; }
dl div { display: flex; gap: 10px; }
dt { width: 70px; color: var(--ink-faint); }
dd { margin: 0; color: var(--ink); }
.actions { display: flex; gap: 8px; }
</style>
