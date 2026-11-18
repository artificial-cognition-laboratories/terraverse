<script setup lang="ts">
/**
 * The server at a glance: its step, the process's CPU and memory, and how much
 * it's simulating, over the last fifteen seconds; then what it is, as facts.
 */
import { computed } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import Details, { type DetailSection } from "../kit/Details.vue"
import { colorOf, IDLE } from "../perf/colors.ts"
import LineChart, { type Series } from "../perf/LineChart.vue"
import type { Window } from "../perf/view.ts"
import type { ServerStoreHandle } from "./store.ts"

const props = defineProps<{ frames: readonly PerfFrame[]; server: ServerStoreHandle }>()

const SPAN = 15_000
const win = computed<Window>(() => {
    const last = props.frames[props.frames.length - 1]
    const end = last ? last.end : 1
    return { startMs: end - SPAN, endMs: end }
})
const shown = computed(() => props.frames.filter((f) => f.end > win.value.startMs - 200))

// The server thread's three costliest zones over what's shown, its waits left out.
const step = computed<Series[]>(() => {
    const sums = new Map<string, number>()
    for (const f of shown.value) for (const [name, ms] of Object.entries(f.threads.Server ?? {})) if (!IDLE.has(name)) sums.set(name, (sums.get(name) ?? 0) + ms)
    return [...sums.entries()].sort((a, b) => b[1] - a[1]).slice(0, 3)
        .map(([name]) => ({ name: name, color: colorOf(name), value: (f: PerfFrame) => f.threads.Server?.[name] ?? 0 }))
})
const counter = (name: string) => (f: PerfFrame) => f.counters[name] ?? Number.NaN
const cpu: Series[] = [{ name: "process", color: "#e6e6ee", value: counter("Process: CPU"), bold: true }]
const memory: Series[] = [{ name: "resident", color: "#4f9dff", value: counter("Process: memory"), bold: true }]
const active: Series[] = [
    { name: "active blocks", color: "#5fbf72", value: counter("ServerEnv: active blocks") },
    { name: "objects", color: "#e5a33b", value: counter("ActiveObjectMgr: SAO count") },
]
const pct = (v: number) => `${v.toFixed(0)}%`
const mb = (v: number) => (v >= 1000 ? `${(v / 1000).toFixed(2)} GB` : `${v.toFixed(0)} MB`)
const count = (v: number) => Math.round(v).toLocaleString()

const sections = computed<DetailSection[]>(() => {
    const s = props.server.summary.value
    const p = props.server.process.value
    const out: DetailSection[] = []
    if (!s) {
        out.push({ title: "Server", about: "The server this game is connected to.", rows: [{ name: "Status", value: "not in a world", dot: "off" }] })
    } else if (!s.local) {
        out.push({ title: "Server", about: "The server this game is connected to.", rows: [
            { name: "Status", note: "only what the connection shows", value: "remote", dot: "warn" },
            { name: "Address", value: s.address, mono: true },
        ] })
    } else {
        out.push({ title: "Server", about: "The server this game is connected to: one this game runs, in its own process.", rows: [
            { name: "Status", value: "local · running in this process", dot: "up" },
            { name: "Game", value: s.game },
            { name: "Version", value: s.version, mono: true },
            { name: "Uptime", value: duration(s.uptime), mono: true },
            { name: "Lag", note: "how far its step has lately run behind", value: `${(s.lag * 1000).toFixed(0)} ms`, mono: true },
            { name: "Players", value: String(s.players.length), mono: true },
            { name: "Mods", value: String(s.mods.length), mono: true },
        ] })
        out.push({ title: "World", about: "Where it keeps the world.", rows: [{ name: "Path", value: s.world, mono: true }] })
    }
    if (p) {
        out.push({ title: "Process", about: "This game's process. A local server shares it, so these figures are the client's too.", rows: [
            { name: "PID", value: String(p.pid), mono: true },
            { name: "CPU", note: "of one core, every thread together", value: `${p.cpu.toFixed(0)}%`, mono: true },
            { name: "Memory", note: "resident", value: mb(p.rss / 1e6), mono: true },
            { name: "Threads", value: p.threads.length ? String(p.threads.length) : "–", mono: true },
        ] })
        const busiest = [...p.threads].sort((a, b) => b.cpu - a.cpu).slice(0, 6)
        if (busiest.length) out.push({ title: "Busiest threads", about: "Where the CPU goes, thread by thread, as the system names them.",
            rows: busiest.map((t) => ({ name: t.name, value: `${t.cpu.toFixed(0)}%`, mono: true })) })
    }
    return out
})
function duration(seconds: number) {
    const h = Math.floor(seconds / 3600)
    const m = Math.floor((seconds % 3600) / 60)
    const s = Math.floor(seconds % 60)
    return h ? `${h} h ${m} m` : m ? `${m} m ${s} s` : `${s} s`
}
</script>

<template>
    <div class="overview">
        <template v-if="frames.length">
            <LineChart v-if="step.length" title="Step" :frames="shown" :window="win" :series="step" :height="70" :cursor="null" />
            <LineChart title="CPU" :frames="shown" :window="win" :series="cpu" :height="56" :cursor="null" :format="pct" />
            <LineChart title="Memory" :frames="shown" :window="win" :series="memory" :height="56" :cursor="null" :format="mb" />
            <LineChart title="Simulating" :frames="shown" :window="win" :series="active" :height="56" :cursor="null" :format="count" />
        </template>
        <Details :sections="sections" />
    </div>
</template>

<style scoped>
.overview { height: 100%; overflow-y: auto; }
</style>
