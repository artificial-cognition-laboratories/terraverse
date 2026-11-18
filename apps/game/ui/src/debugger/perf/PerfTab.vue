<script setup lang="ts">
import { computed, onUnmounted, ref, shallowRef, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import { aggregate, type ZoneRow } from "./aggregate.ts"
import { colorOf } from "./colors.ts"
import FlameGraph from "./FlameGraph.vue"
import Minimap, { type Line } from "./Minimap.vue"
import type { PerfStoreHandle } from "./store.ts"
import { View, type Window } from "./view.ts"

/**
 * Where the time goes: the minimap over every frame the page has, frame time
 * and the main zones as lines, and under it the flame graph of whatever window
 * the minimap holds. Against the newest frame the window rides along (live);
 * move it anywhere else and it stays put. A drawer at the foot sums the window.
 */
const props = defineProps<{ perf: PerfStoreHandle; exportTrace: () => Promise<string> }>()
const perf = props.perf

const bounds = (): Window => {
    const list = perf.frames.value
    const first = list[0]
    const last = list[list.length - 1]
    return first && last ? { startMs: first.start, endMs: last.end } : { startMs: 0, endMs: 1 }
}
// a fresh view opens on the last ~18 frames: each legible, and what it's doing now
const view = View({ bounds: bounds, initialSpanMs: 300 })
const moved = ref(0) // bumped by every gesture: the view itself isn't reactive
const win = computed<Window>(() => {
    void moved.value
    void perf.frames.value
    return view.current
})
const live = computed(() => {
    void moved.value
    return view.following
})
const range = computed(() => {
    void perf.frames.value
    return bounds()
})
// What the graph draws: the window, except while live, when it's the newest
// range it has zones for. The window moves every frame and the zones four
// times a second; drawn against the window, the newest edge would be bare.
const shownWindow = computed<Window>(() => {
    const zones = perf.zones.value
    if (!live.value || !zones) return win.value
    const span = win.value.endMs - win.value.startMs
    return { startMs: zones.range.to - span, endMs: zones.range.to }
})
function gesture(act: () => void) {
    act()
    moved.value++
}

// The zones of the window: asked again as it moves (settled first), and four
// times a second while it rides the newest frames.
const flame = ref<InstanceType<typeof FlameGraph> | null>(null)
let timer: ReturnType<typeof setTimeout> | null = null
let lastAsk = 0
watch(win, (w) => {
    if (timer) clearTimeout(timer)
    const wait = live.value ? Math.max(0, 250 - (performance.now() - lastAsk)) : 90
    timer = setTimeout(() => {
        lastAsk = performance.now()
        void perf.fetch({ from: w.startMs, to: w.endMs }, flame.value?.width ?? 800)
    }, wait)
}, { immediate: true })
onUnmounted(() => {
    if (timer) clearTimeout(timer)
})

// The minimap's lines: frame time, and the zones costing most per frame (all
// that's kept), each one toggled from the legend. A zone picked in the graph is drawn too.
const picked = ref<string | null>(null)
const hidden = ref(new Set<string>(["Wait"]))
const ranked = computed(() => {
    const sums = new Map<string, number>()
    for (const f of perf.frames.value) for (const [name, ms] of Object.entries(f.parts)) sums.set(name, (sums.get(name) ?? 0) + ms)
    return [...sums.entries()].sort((a, b) => b[1] - a[1]).slice(0, 8).map(([name]) => name)
})
const FRAME: Line = { name: "Frame", color: "#e6e6ee", value: (f) => f.end - f.start, bold: true }
const lines = computed<Line[]>(() => {
    const names = ranked.value.filter((n) => !hidden.value.has(n))
    if (picked.value && !names.includes(picked.value)) names.push(picked.value)
    return [FRAME, ...names.map((name) => ({ name: name, color: colorOf(name), value: (f: PerfFrame) => f.parts[name] ?? 0 }))]
})
function toggle(name: string) {
    const next = new Set(hidden.value)
    if (next.has(name)) next.delete(name)
    else next.add(name)
    hidden.value = next
}

// The last second, for the readout.
const recent = computed(() => {
    const list = perf.frames.value
    const last = list[list.length - 1]
    if (!last) return null
    const times = list.filter((f) => f.end > last.end - 1000).map((f) => f.end - f.start).sort((a, b) => a - b)
    return {
        fps: times.length,
        avg: times.reduce((a, b) => a + b, 0) / Math.max(1, times.length),
        p95: times[Math.min(times.length - 1, Math.floor(times.length * 0.95))] ?? 0,
        max: times[times.length - 1] ?? 0,
    }
})
const ms = (v: number) => (v >= 10 ? v.toFixed(1) : v.toFixed(2))

// The drawer: the window summed, by zone.
const open = ref(false)
const rows = shallowRef<ZoneRow[]>([])
watch([() => perf.zones.value, open], () => {
    const zones = perf.zones.value
    rows.value = open.value && zones ? aggregate(zones.data, zones.range.from, zones.range.to).filter((r) => r.name !== "…") : []
})
const framesIn = computed(() => perf.frames.value.filter((f) => f.end > win.value.startMs && f.start < win.value.endMs).length)
const columns: Column<ZoneRow>[] = [
    { id: "name", header: "Zone", flex: 2, value: (r) => r.name, swatch: (r) => colorOf(r.name) },
    { id: "self", header: "Self ms", width: 76, mono: true, value: (r) => ms(r.self) },
    { id: "total", header: "Total ms", width: 80, mono: true, value: (r) => ms(r.total) },
    { id: "calls", header: "Calls", width: 60, mono: true, muted: true, value: (r) => String(r.calls) },
    { id: "max", header: "Max ms", width: 72, mono: true, muted: true, value: (r) => ms(r.max) },
    { id: "threads", header: "Threads", flex: 1, muted: true, value: (r) => r.threads.join(", ") },
]

const exported = ref<string | null>(null)
const exportProblem = ref<string | null>(null)
async function saveTrace() {
    exportProblem.value = null
    try {
        exported.value = await props.exportTrace()
    } catch (error) {
        exportProblem.value = error instanceof Error ? error.message : String(error)
    }
}
</script>

<template>
    <div class="perf">
        <div class="toolbar">
            <button class="mode" :class="{ live: live }" :title="live ? 'Following the newest frames' : 'Back to the newest frames'"
                @click="gesture(() => view.follow())">
                <i />{{ live ? "Live" : "Go live" }}
            </button>
            <span v-if="recent" class="stats">
                <b>{{ recent.fps }}</b> fps · <b>{{ ms(recent.avg) }}</b> avg · <b>{{ ms(recent.p95) }}</b> p95 · <b>{{ ms(recent.max) }}</b> max <em>ms, last second</em>
            </span>
            <span class="gap" />
            <button class="icon" title="Save everything recorded as a Chrome trace (Perfetto, chrome://tracing)" aria-label="Export trace"
                @click="saveTrace">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2v8M4.5 6.5L8 10l3.5-3.5M2.5 11v2.5h11V11" /></svg>
            </button>
        </div>
        <p v-if="exported" class="note" @click="exported = null">Trace saved to <code>{{ exported }}</code></p>
        <p v-if="exportProblem" class="note bad" @click="exportProblem = null">{{ exportProblem }}</p>

        <div class="overview">
            <div class="legend">
                <span class="chip on frame"><i />Frame</span>
                <button v-for="name in ranked" :key="name" class="chip" :class="{ on: !hidden.has(name) }" @click="toggle(name)">
                    <i :style="{ background: colorOf(name) }" />{{ name }}
                </button>
            </div>
            <Minimap v-if="perf.frames.value.length" :frames="perf.frames.value" :bounds="range" :window="win" :lines="lines"
                @show="(a, b) => gesture(() => view.show(a, b))" />
            <p v-else class="waiting">Waiting for frames from the engine…</p>
        </div>

        <p v-if="perf.problem.value" class="note bad">{{ perf.problem.value }}</p>
        <FlameGraph ref="flame" class="graph" :zones="perf.zones.value?.data ?? null" :window="shownWindow" :frames="perf.frames.value" :picked="picked"
            @pan="(px, w) => gesture(() => view.pan(px, w))" @zoom="(s, a) => gesture(() => view.zoom(s, a))"
            @show="(a, b) => gesture(() => view.show(a, b))" @pick="(name) => (picked = name)" />

        <div class="drawer" :class="{ open: open }">
            <button class="drawer-bar" @click="open = !open">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path :d="open ? 'M4 6l4 4 4-4' : 'M4 10l4-4 4 4'" /></svg>
                Summary <span class="faint">· {{ framesIn }} frames · {{ ms(win.endMs - win.startMs) }} ms</span>
                <span v-if="picked" class="faint">· picked {{ picked }}</span>
            </button>
            <div v-if="open" class="table">
                <DataTable :columns="columns" :rows="rows" :row-key="(r) => r.name" :row-class="(r) => (picked === r.name ? 'picked' : undefined)"
                    @select="(r) => (picked = picked === r.name ? null : r.name)" />
            </div>
        </div>
    </div>
</template>

<style scoped>
.perf { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.toolbar { display: flex; align-items: center; gap: 8px; flex: none; height: 32px; padding: 0 6px; border-bottom: 1px solid var(--dt-line); }
button { display: flex; align-items: center; border: 0; background: none; color: var(--dt-dim); font: inherit; cursor: default; }
.mode { gap: 6px; height: 24px; padding: 0 8px; border-radius: 4px; color: var(--dt-ink); }
.mode:hover { background: var(--dt-hover); }
.mode i { width: 8px; height: 8px; border-radius: 50%; background: var(--dt-faint); }
.mode.live i { background: #5fd17a; box-shadow: 0 0 6px #5fd17a; }
.icon { justify-content: center; width: 26px; height: 26px; border-radius: 4px; }
.icon:hover { color: var(--dt-ink); background: var(--dt-hover); }
svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
.stats { min-width: 0; overflow: hidden; color: var(--dt-dim); font-size: 12px; white-space: nowrap; text-overflow: ellipsis; }
.stats b { color: var(--dt-ink); font-weight: 500; font-family: ui-monospace, monospace; }
.stats em { color: var(--dt-faint); font-style: normal; }
.gap { flex: 1; }
.faint { color: var(--dt-faint); }

.overview { flex: none; padding: 4px 6px 6px; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
.legend { display: flex; gap: 2px 10px; flex-wrap: wrap; padding: 2px 2px 5px; font-size: 11px; }
.chip { gap: 5px; padding: 0; color: var(--dt-faint); white-space: nowrap; }
.chip i { width: 10px; height: 3px; border-radius: 1px; background: var(--dt-faint); opacity: 0.4; }
.chip.on { color: var(--dt-dim); }
.chip.on i { opacity: 1; }
.chip:hover { color: var(--dt-ink); }
.chip.frame i { height: 2px; background: #e6e6ee; }
.waiting { height: 72px; margin: 0; display: grid; place-items: center; color: var(--dt-faint); }

.graph { flex: 1; min-height: 0; }

.drawer { flex: none; display: flex; flex-direction: column; border-top: 1px solid var(--dt-line); }
.drawer.open { height: 38%; min-height: 120px; }
.drawer-bar { flex: none; gap: 6px; height: 26px; padding: 0 8px; color: var(--dt-ink); font-size: 12px; background: var(--dt-rail); }
.drawer-bar:hover { background: var(--dt-hover); }
.table { flex: 1; min-height: 0; }
.table :deep(.picked) { background: var(--dt-held); }
.note { margin: 0; padding: 6px 10px; border-bottom: 1px solid var(--dt-line); color: var(--dt-dim); font-size: 12px; cursor: default; }
.note code { color: var(--dt-ink); user-select: text; }
.note.bad { color: var(--dt-error); background: var(--dt-error-bg); }
</style>
