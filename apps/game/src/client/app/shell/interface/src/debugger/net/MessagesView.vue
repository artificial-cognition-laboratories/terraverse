<script setup lang="ts">
/**
 * What crossed the connection over the window the charts show, one row per
 * message type and way: how many, how much, how fast, how big, its share of
 * that way's bytes, and how long since the last. The rate is tinted as it
 * trends (green rising, red falling). Ten times a second. Click a row to pick
 * the type out: its own line in the charts, and the Packets view narrowed to it.
 */
import { computed, onUnmounted, shallowRef } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import { categoryOf, formatBytes, short } from "./categories.ts"

const props = defineProps<{
    frames: readonly PerfFrame[]
    spanMs: number
    nameOf: (dir: 0 | 1, command: number) => string
    text: string
    picked: string | null
}>()
const emit = defineEmits<{ pick: [key: string | null] }>()

type Row = {
    key: string; name: string; dir: 0 | 1; category: ReturnType<typeof categoryOf>
    count: number; bytes: number; largest: number; rate: number; share: number; ago: number; trend: number
}

const rows = shallowRef<Row[]>([])
function refresh() {
    const list = props.frames
    const last = list[list.length - 1]
    if (!last) return
    const from = last.end - props.spanMs
    type Acc = { count: number; bytes: number; largest: number; seen: number; recent: number; before: number }
    const acc = new Map<string, Acc>()
    const wayBytes = [0, 0]
    for (let i = list.length - 1; i >= 0 && list[i]!.end > from; i--) {
        const f = list[i]!
        const age = last.end - f.end
        for (let k = 0; k + 5 <= f.net.length; k += 5) {
            const key = `${f.net[k + 1]}:${f.net[k]}`
            let a = acc.get(key)
            if (!a) acc.set(key, (a = { count: 0, bytes: 0, largest: 0, seen: f.end, recent: 0, before: 0 }))
            const bytes = f.net[k + 3]!
            a.count += f.net[k + 2]!
            a.bytes += bytes
            a.largest = Math.max(a.largest, f.net[k + 4]!)
            if (age < 1000) a.recent += bytes
            else if (age < 2000) a.before += bytes
            wayBytes[f.net[k + 1]!] = (wayBytes[f.net[k + 1]!] ?? 0) + bytes
        }
    }
    const seconds = props.spanMs / 1000
    rows.value = [...acc.entries()].map(([key, a]) => {
        const [way, command] = key.split(":").map(Number) as [0 | 1, number]
        const name = props.nameOf(way, command)
        const change = (a.recent - a.before) / Math.max(a.recent, a.before, 1)
        return {
            key: key, name: name, dir: way, category: categoryOf(name),
            count: a.count, bytes: a.bytes, largest: a.largest, rate: a.bytes / seconds,
            share: a.bytes / Math.max(1, wayBytes[way] ?? 1), ago: (last.end - a.seen) / 1000,
            trend: Math.abs(change) < 0.05 ? 0 : Math.max(-1, Math.min(1, change / 0.5)),
        }
    }).sort((a, b) => b.bytes - a.bytes)
}
refresh()
const ticking = setInterval(refresh, 100)
onUnmounted(() => clearInterval(ticking))

// a new filter whenever the text changes: that's what tells the table to filter again
const filter = computed(() => {
    const needle = props.text.trim().toLowerCase()
    return (row: Row) => !needle || row.name.toLowerCase().includes(needle)
})
const tint = (r: Row) => (r.trend === 0 ? undefined
    : `color-mix(in srgb, ${r.trend > 0 ? "#5fd17a" : "#ff6b6b"} ${Math.round(25 + Math.abs(r.trend) * 75)}%, var(--dt-ink))`)
const columns: Column<Row>[] = [
    { id: "name", header: "Type", flex: 1, minWidth: 150, value: (r) => short(r.name), swatch: (r) => r.category.color },
    { id: "dir", header: "", width: 30, value: (r) => (r.dir === 0 ? "↓" : "↑"), color: (r) => (r.dir === 0 ? "var(--dt-accent)" : "#e5a33b") },
    { id: "rate", header: "Rate", width: 82, mono: true, value: (r) => `${formatBytes(r.rate)}/s`, color: tint },
    { id: "bytes", header: "Bytes", width: 70, mono: true, value: (r) => formatBytes(r.bytes) },
    { id: "count", header: "Count", width: 62, mono: true, muted: true, value: (r) => r.count.toLocaleString() },
    { id: "avg", header: "Avg", width: 62, mono: true, muted: true, value: (r) => formatBytes(r.bytes / Math.max(1, r.count)) },
    { id: "max", header: "Max", width: 62, mono: true, muted: true, value: (r) => formatBytes(r.largest) },
    { id: "share", header: "Share", width: 58, mono: true, value: (r) => `${(r.share * 100).toFixed(r.share < 0.1 ? 1 : 0)}%` },
    { id: "ago", header: "Last", width: 56, mono: true, muted: true, value: (r) => (r.ago < 0.05 ? "now" : `${r.ago.toFixed(1)} s`) },
]
</script>

<template>
    <DataTable class="messages" :columns="columns" :rows="rows" :filter="filter" :row-key="(r) => r.key"
        :row-class="(r) => (picked === r.key ? 'picked' : undefined)" @select="(r) => emit('pick', picked === r.key ? null : r.key)" />
</template>

<style scoped>
.messages { height: 100%; }
.messages :deep(.ag-row.picked)::before { content: ""; position: absolute; top: 0; bottom: 0; left: 0; z-index: 1; width: 2px; background: var(--dt-accent); }
</style>
