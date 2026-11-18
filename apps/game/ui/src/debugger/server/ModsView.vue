<script setup lang="ts">
/**
 * The mods the local server runs: where each came from, how long it took to
 * load (from the startup marks), what it depends on. Select one for the rest.
 */
import { computed, ref } from "vue"
import type { ServerMod } from "../../bridge/index.ts"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import DetailPane from "../kit/DetailPane.vue"
import Details from "../kit/Details.vue"

const props = defineProps<{ mods: readonly ServerMod[]; loads: ReadonlyMap<string, number>; text: string }>()

type Row = ServerMod & { load: number | null }
const rows = computed<Row[]>(() => props.mods.map((m) => ({ ...m, load: props.loads.get(m.name) ?? null })))
const filter = computed(() => {
    const needle = props.text.trim().toLowerCase()
    return (m: Row) => !needle || m.name.toLowerCase().includes(needle) || m.author.toLowerCase().includes(needle)
})
const selected = ref<string | null>(null)
const chosen = computed(() => rows.value.find((m) => m.name === selected.value) ?? null)
const ms = (v: number | null) => (v === null ? "–" : v >= 1000 ? `${(v / 1000).toFixed(2)} s` : `${v.toFixed(v >= 10 ? 0 : 1)} ms`)
const columns: Column<Row>[] = [
    { id: "name", header: "Mod", flex: 1, minWidth: 120, value: (m) => m.name, swatch: (m) => (m.source === "game" ? "#4f9dff" : "#e5a33b") },
    { id: "load", header: "Load", width: 70, mono: true, value: (m) => ms(m.load) },
    { id: "source", header: "From", width: 60, muted: true, value: (m) => (m.source === "game" ? "game" : "user") },
    { id: "deps", header: "Needs", width: 56, mono: true, muted: true, value: (m) => String(m.depends.length) },
    { id: "author", header: "Author", width: 100, muted: true, value: (m) => m.author },
]
</script>

<template>
    <div class="mods">
        <div class="list">
            <DataTable :columns="columns" :rows="rows" :filter="filter" :row-key="(m) => m.name"
                @select="(m) => (selected = selected === m.name ? null : m.name)" />
        </div>
        <DetailPane v-if="chosen" :title="chosen.name" :color="chosen.source === 'game' ? '#4f9dff' : '#e5a33b'" @close="selected = null">
            <Details :sections="[
                { title: chosen.name, about: chosen.description || undefined, rows: [
                    { name: 'Load', note: 'running its init.lua', value: ms(chosen.load), mono: true },
                    { name: 'From', value: chosen.source === 'game' ? 'the game' : 'the player' },
                    { name: 'Author', value: chosen.author || '–' },
                    { name: 'Path', value: chosen.path, mono: true },
                ] },
                { title: 'Needs', about: 'Mods it loads after.', rows: chosen.depends.length ? chosen.depends.map((d) => ({ name: d, value: '' })) : [{ name: 'Nothing', value: '' }] },
            ]" />
            <p class="todo">Not wired yet: its time per step, and what it registered.</p>
        </DetailPane>
    </div>
</template>

<style scoped>
.mods { display: flex; height: 100%; min-height: 0; }
.list { flex: 1; min-width: 0; }
.todo { margin: 6px 10px; color: var(--dt-faint); font-size: 11px; font-style: italic; }
</style>
