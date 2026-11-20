<script setup lang="ts">
/**
 * The old profiler's pages, as one table: every value reported over the last
 * fifteen seconds, timings and counters alike, sorted by group (names carry
 * their group's prefix; the filter matches groups too), with what it is now,
 * its average and its most, tinted green while it rises and red while it
 * falls (the stronger, the faster), ten times a second. Click
 * a row to pin it: pinned values are charted together at the top of Live.
 */
import { computed, onUnmounted, shallowRef } from "vue"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import { colorOf } from "./colors.ts"
import { formatValue, groupOf } from "./counters.ts"
import type { PerfStoreHandle } from "./store.ts"

const props = defineProps<{ perf: PerfStoreHandle; text: string }>()
const perf = props.perf

const WINDOW_MS = 15_000
/** `trend`: -1 falling fast … 0 steady … 1 rising fast (the last second against the one before). */
type Stat = { name: string; group: string; unit: string; now: number; avg: number; max: number; trend: number }

// A change of a quarter in a second reads at full strength; anything under a
// few percent reads as steady.
const FULL = 0.25
const STEADY = 0.03
function trendOf(recent: number, before: number) {
    if (!Number.isFinite(recent) || !Number.isFinite(before)) return 0
    const change = (recent - before) / Math.max(Math.abs(before), Math.abs(recent), 1e-9)
    if (Math.abs(change) < STEADY) return 0
    return Math.max(-1, Math.min(1, change / FULL))
}

const rows = shallowRef<Stat[]>([])
function refresh() {
    const list = perf.frames.value
    const last = list[list.length - 1]
    if (!last) return
    type Acc = { now: number; sum: number; max: number; frames: number; recent: [number, number]; before: [number, number] }
    const stats = new Map<string, Acc>()
    for (let i = list.length - 1; i >= 0; i--) {
        const f = list[i]!
        const age = last.end - f.end
        if (age > WINDOW_MS) break
        for (const [name, v] of Object.entries(f.counters)) {
            let s = stats.get(name)
            if (!s) stats.set(name, (s = { now: v, sum: 0, max: v, frames: 0, recent: [0, 0], before: [0, 0] })) // newest first: the first met is now
            s.sum += v
            s.max = Math.max(s.max, v)
            s.frames++
            if (age < 1000) {
                s.recent[0] += v
                s.recent[1]++
            } else if (age < 2000) {
                s.before[0] += v
                s.before[1]++
            }
        }
    }
    rows.value = [...stats.entries()].map(([name, s]) => ({
        name: name, group: groupOf(name).title, unit: perf.units.value[name] ?? "",
        now: s.now, avg: s.sum / s.frames, max: s.max,
        trend: trendOf(s.recent[0] / s.recent[1], s.before[0] / s.before[1]),
    })).sort((a, b) => a.group.localeCompare(b.group) || a.name.localeCompare(b.name))
}
refresh()
const ticking = setInterval(refresh, 100)
onUnmounted(() => clearInterval(ticking))

// the filter's text comes from the Performance tab's row, where its box sits
const filter = computed(() => {
    const needle = props.text.trim().toLowerCase()
    return needle ? (row: Stat) => row.name.toLowerCase().includes(needle) || row.group.toLowerCase().includes(needle) : undefined
})

// Green rising, red falling, the ink when steady; how far from the ink is how fast.
const tint = (r: Stat) => {
    if (r.trend === 0) return undefined
    const share = Math.round(25 + Math.abs(r.trend) * 75)
    return `color-mix(in srgb, ${r.trend > 0 ? "#5fd17a" : "#ff6b6b"} ${share}%, var(--dt-ink))`
}
const columns: Column<Stat>[] = [
    { id: "name", header: "Value", flex: 1, value: (r) => r.name, swatch: (r) => colorOf(r.name) },
    { id: "now", header: "Now", width: 72, mono: true, value: (r) => formatValue(r.now, r.unit), color: tint },
    { id: "avg", header: "Avg", width: 72, mono: true, value: (r) => formatValue(r.avg, r.unit), color: tint },
    { id: "max", header: "Max", width: 72, mono: true, value: (r) => formatValue(r.max, r.unit), color: tint },
]
// The grid redraws a row when its object changes: new ones, so the mark shows now.
function pin(row: Stat) {
    perf.pin(row.name)
    rows.value = rows.value.map((r) => ({ ...r }))
}
</script>

<template>
    <div class="stats">
        <DataTable class="table" :columns="columns" :rows="rows" :filter="filter" :row-key="(r) => r.name"
            :row-class="(r) => (perf.pinned.value.includes(r.name) ? 'pinned' : undefined)" @select="pin" />
    </div>
</template>

<style scoped>
.stats { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.table { flex: 1; min-height: 0; }
/* pinned: charted in Live; an accent edge rather than a column of its own */
.table :deep(.ag-row.pinned)::before { content: ""; position: absolute; top: 0; bottom: 0; left: 0; z-index: 1; width: 2px; background: var(--dt-accent); }
</style>
