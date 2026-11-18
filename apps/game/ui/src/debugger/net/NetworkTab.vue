<script setup lang="ts">
/**
 * The client's connection: live charts over the newest frames on top (what's
 * coming down and going up by category, messages a second, the connection's
 * round trip, and its moments), and below, the detail: Messages (every type,
 * summed over what the charts show), Packets (a capture, every message) or
 * Join (the last join, as a waterfall). The two halves are one: click a
 * moment on a chart to capture it, pick a type to see it in the charts, and
 * the filter narrows the tables. The split between them drags.
 */
import { computed, ref } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import LineChart, { type Series } from "../perf/LineChart.vue"
import type { PerfStoreHandle } from "../perf/store.ts"
import type { Window } from "../perf/view.ts"
import { CATEGORIES, formatBytes, OTHER } from "./categories.ts"
import EventLane from "./EventLane.vue"
import JoinView from "./JoinView.vue"
import MessagesView from "./MessagesView.vue"
import PacketsView from "./PacketsView.vue"
import { rates } from "./rates.ts"
import type { NetStoreHandle } from "./store.ts"

const props = defineProps<{ perf: PerfStoreHandle; net: NetStoreHandle }>()
const net = props.net

// Always the newest frames; the wheel sets how much time that is.
const SPAN = { least: 1000, most: 15_000 }
const span = ref(SPAN.most)
const frames = computed(() => props.perf.frames.value)
const win = computed<Window>(() => {
    const last = frames.value[frames.value.length - 1]
    const end = last ? last.end : 1
    return { startMs: end - span.value, endMs: end }
})
// the rates of the frames in view (a little before, for the first's window)
const shown = computed(() => {
    const list = frames.value
    let i = list.length
    while (i > 0 && list[i - 1]!.end > win.value.startMs - 500) i--
    return list.slice(i)
})
const rated = computed(() => rates(shown.value, net.nameOf))

const ALL = [...CATEGORIES, OTHER]
const shownCategories = ALL

const picked = ref<string | null>(null)
const pickedName = computed(() => {
    if (!picked.value) return null
    const [way, command] = picked.value.split(":").map(Number) as [0 | 1, number]
    return net.nameOf(way, command)
})

const kB = (bytes: number) => bytes / 1000
const rate = (f: PerfFrame) => rated.value.get(f.id)
const pickedLine = (way: 0 | 1): Series[] => (picked.value && pickedName.value && Number(picked.value.split(":")[0]) === way
    ? [{ name: `▸ ${pickedName.value.replace(/^TO(CLIENT|SERVER)_/, "")}`, color: "#ffffff", value: (f) => kB(rate(f)?.byType.get(picked.value!) ?? 0), bold: true }]
    : [])
const down = computed<Series[]>(() => [
    ...pickedLine(0),
    ...shownCategories.map((c): Series => ({ name: c.label, color: c.color, value: (f) => kB(rate(f)?.down[c.id] ?? 0) })),
])
const up = computed<Series[]>(() => [
    ...pickedLine(1),
    ...shownCategories.map((c): Series => ({ name: c.label, color: c.color, value: (f) => kB(rate(f)?.up[c.id] ?? 0) })),
])
const msgs = computed<Series[]>(() => shownCategories.map((c): Series => ({
    name: c.label, color: c.color, value: (f) => (rate(f)?.downMsgs[c.id] ?? 0) + (rate(f)?.upMsgs[c.id] ?? 0),
})))
const counter = (name: string) => (f: PerfFrame) => f.counters[name] ?? Number.NaN
const connection: Series[] = [
    { name: "RTT", color: "#e6e6ee", value: counter("Net: RTT"), bold: true },
    { name: "jitter", color: "#8a8cf0", value: counter("Net: jitter") },
]
const lost: Series[] = [{ name: "lost", color: "#ff6b6b", value: (f) => (f.counters["Net: lost"] ?? Number.NaN) * 1.024 }]
const asRate = (v: number) => `${v >= 100 ? v.toFixed(0) : v.toFixed(1)} kB/s`
const asCount = (v: number) => `${Math.round(v).toLocaleString()}/s`
const asMs = (v: number) => `${v.toFixed(v >= 10 ? 0 : 1)} ms`

// The readouts: the newest rate, and the connection now.
const now = computed(() => {
    const last = shown.value[shown.value.length - 1]
    const r = last ? rate(last) : undefined
    return {
        down: r ? formatBytes(r.downAll) : "–", up: r ? formatBytes(r.upAll) : "–",
        rtt: last && last.counters["Net: RTT"] !== undefined ? asMs(last.counters["Net: RTT"]) : "–",
    }
})

const box = ref<HTMLElement | null>(null)
const cursor = ref<number | null>(null)
const msAt = (event: MouseEvent) => {
    const rect = box.value?.getBoundingClientRect()
    if (!rect) return null
    return win.value.startMs + ((event.clientX - rect.left) / rect.width) * (win.value.endMs - win.value.startMs)
}
const newest = () => frames.value[frames.value.length - 1]?.end ?? null
const CAPTURE_MS = 3000
const view = ref<"messages" | "packets" | "join">("messages")
async function captureAt(at: number | null) {
    const last = frames.value[frames.value.length - 1]
    if (!last) return
    const to = at === null ? last.end : Math.min(last.end, at + CAPTURE_MS / 3)
    view.value = "packets"
    await net.capture(Math.max(0, to - CAPTURE_MS), to)
}
function onWheel(event: WheelEvent) {
    span.value = Math.min(SPAN.most, Math.max(SPAN.least, span.value * Math.exp(event.deltaY * 0.0015)))
}
const spanLabel = computed(() => `${(span.value / 1000).toFixed(span.value >= 10_000 ? 0 : 1)} s`)

// The split: the charts' height, dragged.
const chartsHeight = ref(360)
function grab(event: MouseEvent) {
    const startY = event.clientY
    const startH = chartsHeight.value
    const move = (e: MouseEvent) => (chartsHeight.value = Math.max(120, Math.min(900, startH + e.clientY - startY)))
    const up = () => {
        window.removeEventListener("mousemove", move)
        window.removeEventListener("mouseup", up)
    }
    window.addEventListener("mousemove", move)
    window.addEventListener("mouseup", up)
}
const text = ref("")
</script>

<template>
    <div class="network">
        <div class="bar">
            <span class="read"><b class="down">↓ {{ now.down }}/s</b><b class="up">↑ {{ now.up }}/s</b><span>RTT <b>{{ now.rtt }}</b></span></span>
            <span class="gap" />
            <span class="span" title="The time the charts show: the wheel over them sets it">{{ spanLabel }}</span>
            <button class="action" :disabled="net.capturing.value || !frames.length" title="Every message over the last 3 seconds" @click="captureAt(null)">
                <svg viewBox="0 0 16 16" aria-hidden="true"><circle cx="8" cy="8" r="5.5" /><circle cx="8" cy="8" r="2.2" class="dot" /></svg>
                {{ net.capturing.value ? "Capturing…" : "Capture" }}
            </button>
        </div>
        <p v-if="net.problem.value" class="note bad" @click="net.problem.value = null">{{ net.problem.value }}</p>

        <div v-if="frames.length" ref="box" class="charts" :style="{ height: `${chartsHeight}px` }"
            @mousemove="(e: MouseEvent) => (cursor = msAt(e))" @mouseleave="cursor = null" @click="(e: MouseEvent) => captureAt(msAt(e))" @wheel.prevent="onWheel">
            <LineChart title="↓ Down" :frames="shown" :window="win" :series="down" :height="84" :cursor="cursor" :format="asRate" />
            <LineChart title="↑ Up" :frames="shown" :window="win" :series="up" :height="64" :cursor="cursor" :format="asRate" />
            <LineChart title="Messages" :frames="shown" :window="win" :series="msgs" :height="64" :cursor="cursor" :format="asCount" />
            <LineChart title="Connection" :frames="shown" :window="win" :series="connection" :height="56" :cursor="cursor" :format="asMs" />
            <LineChart title="Lost" :frames="shown" :window="win" :series="lost" :height="44" :cursor="cursor" :format="asRate" />
            <EventLane :events="net.events.value" :window="win" :name-of="net.nameOf" />
        </div>
        <p v-else class="waiting">Waiting for frames from the engine…</p>
        <div class="split" title="Drag to resize" @mousedown.left.prevent="grab" />

        <div class="detail-bar">
            <nav role="tablist" aria-label="Network views">
                <button role="tab" :aria-selected="view === 'messages'" :class="{ on: view === 'messages' }" @click="view = 'messages'">Messages</button>
                <button role="tab" :aria-selected="view === 'packets'" :class="{ on: view === 'packets' }" @click="view = 'packets'">Packets</button>
                <button role="tab" :aria-selected="view === 'join'" :class="{ on: view === 'join' }" @click="view = 'join'">Join</button>
            </nav>
            <span class="gap" />
            <label class="search">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M2 3h12l-4.5 5.5V13l-3-1.5v-3z" /></svg>
                <input v-model="text" type="search" placeholder="Filter types" aria-label="Filter types" spellcheck="false" />
            </label>
        </div>
        <div class="detail">
            <MessagesView v-if="view === 'messages'" :frames="frames" :span-ms="span" :name-of="net.nameOf" :text="text"
                :picked="picked" @pick="(k) => (picked = k)" />
            <PacketsView v-else-if="view === 'packets'" :net="net" :text="text" :picked="picked" :now="newest" />
            <JoinView v-else :events="net.events.value" :name-of="net.nameOf" />
        </div>
    </div>
</template>

<style scoped>
.network { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.bar { display: flex; align-items: center; gap: 8px; flex: none; height: 30px; padding: 0 6px 0 10px; border-bottom: 1px solid var(--dt-line); font-size: 12px; }
.read { display: flex; gap: 14px; min-width: 0; overflow: hidden; color: var(--dt-dim); white-space: nowrap; }
.read b { font: 500 12px ui-monospace, monospace; color: var(--dt-ink); }
.read .down { color: var(--dt-accent); }
.read .up { color: #e5a33b; }
.gap { flex: 1; }
.span { color: var(--dt-faint); font: 11px ui-monospace, monospace; white-space: nowrap; }
button { display: flex; align-items: center; border: 0; background: none; color: var(--dt-dim); font: inherit; font-size: 12px; cursor: default; }
.action { gap: 6px; height: 24px; padding: 0 8px; border-radius: 4px; color: var(--dt-ink); }
.action:hover:not(:disabled) { background: var(--dt-hover); }
.action:disabled { opacity: 0.5; }
svg { width: 16px; height: 16px; flex: none; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
svg .dot { fill: #ff6b6b; stroke: none; }
.charts { flex: none; overflow-y: auto; cursor: crosshair; }
.waiting { flex: none; height: 120px; display: grid; place-items: center; margin: 0; color: var(--dt-faint); }
.split { flex: none; height: 4px; margin-top: -2px; cursor: row-resize; border-top: 1px solid var(--dt-line); position: relative; z-index: 2; }
.split:hover { background: var(--dt-accent); }

.detail-bar { display: flex; align-items: center; gap: 8px; flex: none; min-width: 0; height: 30px; padding-right: 6px; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
nav { display: flex; align-self: stretch; flex: none; }
[role=tab] { position: relative; padding: 0 12px; }
[role=tab]:hover { color: var(--dt-ink); background: var(--dt-hover); }
[role=tab].on { color: var(--dt-ink); }
[role=tab].on::after { content: ""; position: absolute; left: 8px; right: 8px; bottom: 0; height: 2px; background: var(--dt-accent); }
.search { display: flex; flex: none; align-items: center; gap: 4px; width: 150px; height: 22px; padding: 0 6px; border-radius: 4px; color: var(--dt-faint); }
.search:focus-within { background: var(--dt-hover); outline: 1px solid var(--dt-accent); }
input { flex: 1; min-width: 0; border: 0; outline: 0; background: none; color: var(--dt-ink); font: inherit; font-size: 12px; }
input::placeholder { color: var(--dt-faint); }
.detail { flex: 1; min-height: 0; }
.note { margin: 0; padding: 6px 10px; border-bottom: 1px solid var(--dt-line); font-size: 12px; cursor: default; }
.note.bad { color: var(--dt-error); background: var(--dt-error-bg); }
</style>
