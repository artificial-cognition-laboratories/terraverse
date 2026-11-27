<script setup lang="ts">
/**
 * Where the memory goes. Live, three wholes and their parts: RAM (the game's
 * processes out of the machine's, everything in use as a line, the game's swap
 * hung below), the engine's heap (each system's share, the unclaimed rest on
 * top in grey: where an unmeasured leak shows) and the graphics card (the
 * game, other apps, free). Then each system on its own chart, a line scaled
 * to its window, as Performance shows a zone. Entering and leaving a world are
 * marked down every chart. Table: every figure now, at its peak, and how fast
 * it's growing, fastest first. The download writes a report an agent reads
 * (summary.md, samples.jsonl) and copies its path.
 */
import { computed, ref } from "vue"
import ReportButton from "../kit/ReportButton.vue"
import SubTabs from "../kit/SubTabs.vue"
import LineChart, { type Series } from "../perf/LineChart.vue"
import { colorOf } from "../perf/colors.ts"
import { formatBytes } from "../perf/counters.ts"
import { smoothEnd } from "../perf/smoothEnd.ts"
import type { ServerStoreHandle } from "../server/store.ts"
import { figuresOf, gameSwap, gpuBands, heapBands, machineUsed, ramBands, readingOf, systemFigure, systemsOf } from "./figures.ts"
import StackedChart from "./StackedChart.vue"
import type { MemoryStoreHandle, Sampled } from "./store.ts"

const props = defineProps<{ memory: MemoryStoreHandle; server: ServerStoreHandle }>()

const VIEWS = [{ id: "live", label: "Live" }, { id: "table", label: "Table" }] as const
const view = ref<(typeof VIEWS)[number]["id"]>("live")
const SPANS = [{ ms: 30_000, label: "30 s" }, { ms: 60_000, label: "1 min" }, { ms: 300_000, label: "5 min" }] as const
const spanMs = ref<number>(30_000)
/** How far back a growth rate looks: long enough to see past a burst. */
const GROWTH_MS = 60_000

const samples = computed(() => props.memory.samples.value)
const last = computed(() => samples.value[samples.value.length - 1])
const end = smoothEnd(() => last.value?.end)
const win = computed(() => ({ startMs: end.value - spanMs.value, endMs: end.value }))
const marks = computed(() => props.server.marks.value.filter((m) => m.lane === "world").map((m) => ({ ms: m.start, label: m.what })))
const cursor = ref<number | null>(null)

const systemColor = (system: string) => colorOf(`memory: ${system}`)
const systems = computed(() => systemsOf(samples.value))
const ram = ramBands()
const heap = computed(() => heapBands(systems.value, systemColor))
const gpu = gpuBands()
const machineLine = { name: "machine in use", value: machineUsed }
const swapBelow = { name: "swapped to disk", value: gameSwap }

/** Each system's own chart: its whole, scaled to the window; its count in the title. */
const systemCharts = computed(() => systems.value.map((system) => {
    const figure = systemFigure(system)
    const count = last.value && figure.count ? figure.count(last.value) : 0
    const series: Series<Sampled>[] = [{ name: "held", color: systemColor(system), value: figure.value, unit: "B" }]
    return { system: system, title: count ? `${system} · ${count.toLocaleString()}` : system, series: series }
}))

const rows = computed(() => {
    const since = (last.value?.t ?? 0) - GROWTH_MS
    return figuresOf(samples.value).map((f) => ({ figure: f, reading: readingOf(f, samples.value, since) }))
        .sort((a, b) => b.reading.perMinute - a.reading.perMinute)
})
const rate = (perMinute: number) => `${perMinute >= 0 ? "+" : "−"}${formatBytes(Math.abs(perMinute))}/min`
/** Growing by more than a megabyte a minute over the last minute: worth a look. */
const growing = (perMinute: number) => perMinute > 1e6

function msAt(e: MouseEvent) {
    const el = e.currentTarget as HTMLElement
    const box = el.getBoundingClientRect()
    return win.value.startMs + ((e.clientX - box.left) / Math.max(box.width, 1)) * (win.value.endMs - win.value.startMs)
}
</script>

<template>
    <div class="memory">
        <div class="bar">
            <SubTabs v-model="view" :tabs="VIEWS" label="Memory views" />
            <span class="gap" />
            <template v-if="view === 'live'">
                <button v-for="s in SPANS" :key="s.ms" class="span" :class="{ on: spanMs === s.ms }" @click="spanMs = s.ms">{{ s.label }}</button>
            </template>
            <ReportButton :save="memory.report" title="Save the last hour as a report (summary.md: what grew, what peaked, what a world kept; samples.jsonl)" />
        </div>

        <div v-if="!samples.length" class="empty">
            <p>No samples yet.</p>
            <p class="faint">The first comes as the game starts.</p>
        </div>
        <div v-else-if="view === 'live'" class="live" @mousemove="(e: MouseEvent) => (cursor = msAt(e))" @mouseleave="cursor = null">
            <StackedChart title="RAM" :samples="samples" :window="win" :bands="ram" :whole="(s) => s.total" :line="machineLine" :below="swapBelow"
                unknown="The machine's memory isn't read here (Linux only, for now)." :cursor="cursor" :marks="marks" :height="120" />
            <StackedChart title="Heap (as malloc counts: the driver's unused reservations in unaccounted)" :samples="samples" :window="win" :bands="heap" :whole="(s) => s.heap"
                unknown="The engine's heap isn't read here (Linux only, for now)." :cursor="cursor" :marks="marks" :height="100" />
            <StackedChart title="GPU" :samples="samples" :window="win" :bands="gpu" :whole="(s) => s.gpuTotal"
                unknown="The graphics card isn't read here: NVIDIA's driver only, for now." :cursor="cursor" :marks="marks" :height="80" />
            <LineChart v-for="c in systemCharts" :key="c.system" :title="c.title" :frames="samples" :window="win" :series="c.series" :height="64"
                :cursor="cursor" :marks="marks" bounds />
        </div>
        <div v-else class="table">
            <table>
                <thead><tr><th>Figure</th><th>Now</th><th>Peak</th><th>Start</th><th>Last min</th><th>Count</th></tr></thead>
                <tbody>
                    <tr v-for="r in rows" :key="`${r.figure.kind}:${r.figure.name}`" :class="{ hot: growing(r.reading.perMinute) }">
                        <td>{{ r.figure.name }}<span class="kind">{{ r.figure.kind }}</span></td>
                        <td>{{ formatBytes(r.reading.now) }}</td>
                        <td>{{ formatBytes(r.reading.peak) }}</td>
                        <td>{{ formatBytes(r.reading.start) }}</td>
                        <td>{{ rate(r.reading.perMinute) }}</td>
                        <td>{{ Number.isNaN(r.reading.count) ? "" : r.reading.count.toLocaleString() }}</td>
                    </tr>
                </tbody>
            </table>
        </div>
    </div>
</template>

<style scoped>
.memory { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.bar { display: flex; align-items: center; gap: 4px; flex: none; height: 30px; padding: 0 6px 0 0; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
.gap { flex: 1; }
.span { height: 22px; padding: 0 7px; border: 0; border-radius: 4px; background: none; color: var(--dt-faint); font: 11px ui-monospace, monospace; cursor: default; }
.span:hover { color: var(--dt-ink); background: var(--dt-hover); }
.span.on { color: var(--dt-ink); }
.live { flex: 1; min-height: 0; overflow-y: auto; cursor: crosshair; }
.table { flex: 1; min-height: 0; overflow: auto; }
table { width: 100%; border-collapse: collapse; font-size: 12px; }
th { position: sticky; top: 0; padding: 4px 8px; background: var(--dt-rail); color: var(--dt-dim); font-weight: 500; text-align: right; border-bottom: 1px solid var(--dt-line); }
td { padding: 3px 8px; color: var(--dt-ink); font-family: ui-monospace, monospace; text-align: right; white-space: nowrap; border-bottom: 1px solid var(--dt-line); }
th:first-child, td:first-child { text-align: left; font-family: inherit; min-width: 0; }
.kind { margin-left: 6px; color: var(--dt-faint); font-size: 10px; }
tr.hot td { color: var(--dt-warn, #e5a33b); }
.empty { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 40px 16px; color: var(--dt-dim); text-align: center; }
.empty p { margin: 0; }
.faint { color: var(--dt-faint); }
</style>
