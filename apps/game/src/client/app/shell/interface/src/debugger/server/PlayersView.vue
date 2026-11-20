<script setup lang="ts">
/** Who's on the local server: their ping, where they are, their health, how long they've been on. Select one for the rest. */
import { computed, ref } from "vue"
import type { ServerPlayer } from "../../bridge/index.ts"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import DetailPane from "../kit/DetailPane.vue"
import Details from "../kit/Details.vue"

const props = defineProps<{ players: readonly ServerPlayer[]; text: string }>()

const rows = computed(() => [...props.players])
const filter = computed(() => {
    const needle = props.text.trim().toLowerCase()
    return (p: ServerPlayer) => !needle || p.name.toLowerCase().includes(needle)
})
const selected = ref<string | null>(null)
const chosen = computed(() => props.players.find((p) => p.name === selected.value) ?? null)
const at = (p: ServerPlayer) => p.position.map((v) => Math.round(v)).join(", ")
const since = (s: number) => (s >= 3600 ? `${Math.floor(s / 3600)} h ${Math.floor((s % 3600) / 60)} m` : s >= 60 ? `${Math.floor(s / 60)} m` : `${s} s`)
const columns: Column<ServerPlayer>[] = [
    { id: "name", header: "Player", flex: 1, minWidth: 120, value: (p) => p.name },
    { id: "rtt", header: "Ping", width: 70, mono: true, value: (p) => (p.rtt === null ? "–" : `${p.rtt.toFixed(1)} ms`) },
    { id: "pos", header: "Position", width: 150, mono: true, muted: true, value: at },
    { id: "hp", header: "HP", width: 50, mono: true, value: (p) => String(p.hp) },
    { id: "on", header: "On for", width: 70, mono: true, muted: true, value: (p) => since(p.connected) },
]
</script>

<template>
    <div class="players">
        <div class="list">
            <DataTable :columns="columns" :rows="rows" :filter="filter" :row-key="(p) => p.name"
                @select="(p) => (selected = selected === p.name ? null : p.name)" />
        </div>
        <DetailPane v-if="chosen" :title="chosen.name" @close="selected = null">
            <Details :sections="[{ title: 'Player', rows: [
                { name: 'Position', value: at(chosen), mono: true },
                { name: 'HP', value: String(chosen.hp), mono: true },
                { name: 'Ping', value: chosen.rtt === null ? '–' : `${chosen.rtt.toFixed(1)} ms`, mono: true },
                { name: 'On for', value: since(chosen.connected), mono: true },
                { name: 'Protocol', value: String(chosen.protocol), mono: true },
            ] }]" />
            <p class="todo">Not wired yet: privileges, and the blocks queued to send this player.</p>
        </DetailPane>
    </div>
</template>

<style scoped>
.players { display: flex; height: 100%; min-height: 0; }
.list { flex: 1; min-width: 0; }
.todo { margin: 6px 10px; color: var(--dt-faint); font-size: 11px; font-style: italic; }
</style>
