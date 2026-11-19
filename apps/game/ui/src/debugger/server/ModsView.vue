<script setup lang="ts">
/**
 * The TypeScript mods the local server runs (its server folder's, as prepared): which,
 * at what version, and whether they're the game's own. Select one for the rest.
 */
import { computed, ref } from "vue"
import type { ServerMod } from "../../bridge/index.ts"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import DetailPane from "../kit/DetailPane.vue"
import Details from "../kit/Details.vue"

const props = defineProps<{ mods: readonly ServerMod[]; text: string }>()

type Row = ServerMod
const rows = computed<Row[]>(() => [...props.mods])
const filter = computed(() => {
    const needle = props.text.trim().toLowerCase()
    return (m: Row) => !needle || m.name.toLowerCase().includes(needle)
})
const selected = ref<string | null>(null)
const chosen = computed(() => rows.value.find((m) => m.name === selected.value) ?? null)
const columns: Column<Row>[] = [
    { id: "name", header: "Mod", flex: 1, minWidth: 120, value: (m) => m.name, swatch: (m) => (m.source === "game" ? "#4f9dff" : "#e5a33b") },
    { id: "source", header: "From", width: 60, muted: true, value: (m) => (m.source === "game" ? "game" : "user") },
    { id: "version", header: "Version", width: 80, mono: true, muted: true, value: (m) => m.version },
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
                { title: chosen.name, rows: [
                    { name: 'From', value: chosen.source === 'game' ? 'the game' : 'the player' },
                    { name: 'Version', value: chosen.version, mono: true },
                ] },
            ]" />
            <p class="todo">Not wired yet: its time per step, what it registered, and what it depends on.</p>
        </DetailPane>
    </div>
</template>

<style scoped>
.mods { display: flex; height: 100%; min-height: 0; }
.list { flex: 1; min-width: 0; }
.todo { margin: 6px 10px; color: var(--dt-faint); font-size: 11px; font-style: italic; }
</style>
