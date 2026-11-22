<script setup lang="ts">
import { computed, onMounted } from "vue"
import type { PublicServer } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { Monogram, Notice } from "../kit/index.ts"
import { Request } from "./library/request.ts"

/** Public servers anyone can join, as rows in the Play list: busiest first, the chosen one picked. */
const props = defineProps<{ query: string }>()
const selected = defineModel<PublicServer | null>({ required: true })
const emit = defineEmits<{ join: [server: PublicServer] }>()
const bridge = useBridge()
const directory = Request(() => bridge.send("servers.list", {}))
const servers = computed(() => {
    const term = props.query.trim().toLowerCase()
    return (directory.data.value?.servers ?? [])
        .filter((s) => `${s.name} ${s.host} ${s.tags.join(" ")} ${s.region}`.toLowerCase().includes(term))
        .sort((a, b) => Number(b.favorite) - Number(a.favorite) || b.players - a.players)
})
const groups = computed(() => [
    { label: "Favourites", servers: servers.value.filter((s) => s.favorite) },
    { label: "Public servers", servers: servers.value.filter((s) => !s.favorite) },
].filter((g) => g.servers.length))
onMounted(async () => {
    await directory.run(undefined)
    if (!selected.value) selected.value = servers.value[0] ?? null
})
</script>

<template>
    <div v-if="directory.busy.value && !directory.data.value" class="list-message">Finding servers…</div>
    <div v-else-if="directory.problem.value" class="list-message"><Notice tone="danger">{{ directory.problem.value }}</Notice></div>
    <div v-else-if="directory.data.value?.status === 'unavailable'" class="list-message">
        <Notice tone="warn">Not wired yet: public servers aren't connected. Friends' open worlds are under Friends.</Notice>
    </div>
    <template v-else-if="groups.length">
        <section v-for="group in groups" :key="group.label" class="list-group">
            <h3>{{ group.label }}</h3>
            <button v-for="s in group.servers" :key="s.id" class="entry" :class="{ on: selected?.id === s.id, old: !s.compatible }"
                @click="selected = s" @dblclick="s.compatible && emit('join', s)">
                <Monogram :name="s.name" :size="40" />
                <span class="copy">
                    <strong>{{ s.name }}</strong>
                    <span class="tags"><span v-for="t in s.tags.slice(0, 3)" :key="t">{{ t }}</span></span>
                    <small>{{ s.host }} · {{ s.region }}<template v-if="s.pingMs !== null"> · {{ s.pingMs }} ms</template></small>
                </span>
                <span class="players" :class="{ full: s.players >= s.capacity }">{{ s.compatible ? `${s.players}/${s.capacity}` : "Other version" }}</span>
            </button>
        </section>
    </template>
    <div v-else class="list-message">{{ query ? "No matches." : "No public servers are open right now." }}</div>
</template>

<style scoped>
/* as the Play list's own rows (play/WorldList.vue) */
.list-group { margin: 0.75rem 0 1rem; }
.list-group h3 { margin: 0 0 0.4rem; padding: 0 0.65rem; color: var(--ink-faint); font: 400 0.62rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
.entry { display: grid; grid-template-columns: 40px minmax(0, 1fr) auto; align-items: center; gap: 0.75rem; width: 100%; min-width: 0; padding: 0.6rem 0.65rem; border: 0; background: none; box-shadow: inset 2px 0 0 transparent; color: var(--ink); text-align: left; transition: background 0.1s; }
.entry:hover { background: rgb(231 237 244 / 0.04); }
.entry.on { background: rgb(99 163 233 / 0.07); box-shadow: inset 2px 0 0 var(--accent-hi); }
.entry.on strong { color: var(--accent-hi); }
.entry.old { opacity: .55; }
.copy { display: flex; flex-direction: column; gap: 0.3rem; min-width: 0; }
.copy strong { overflow: hidden; font: 400 0.75rem var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; text-overflow: ellipsis; white-space: nowrap; }
.copy small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--ink-faint); font-size: 0.68rem; }
.tags { display: flex; gap: 0.5rem; color: var(--ink-dim); font: 0.58rem var(--font-display); text-transform: uppercase; white-space: nowrap; overflow: hidden; }
.players { align-self: start; color: var(--alive); font: 0.62rem var(--font-display); white-space: nowrap; }
.players.full { color: var(--ink-faint); }
.entry.old .players { color: var(--warn); }
.list-message { display: grid; gap: 0.6rem; justify-items: start; padding: 1rem 0.65rem; color: var(--ink-dim); font: 0.82rem/1.5 var(--font-sans); }
</style>
