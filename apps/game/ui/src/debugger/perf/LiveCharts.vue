<script setup lang="ts">
/**
 * The Live tab: what the frames cost as they stream, smooth because it's drawn
 * from the frames alone (no zones fetched). Frame time on top, then a chart
 * per kind of thread (each kind summed) with its costliest zones as lines, then
 * the counters by group. Always the newest frames: the wheel only sets how
 * many seconds that is. Click a moment to capture it for the Flame tab.
 *
 * Charts and their lines hold still. A chart, once shown, stays, in a fixed
 * order; a line, once chosen, stays, and new ones only fill a chart's free
 * places. A line with nothing for a while is a gap, not a reason to move.
 */
import { wheelDelta } from "./wheel.ts"
import { computed, ref, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import { threadOrder } from "./capture.ts"
import { colorOf, IDLE } from "./colors.ts"
import { GROUPS, groupOf, TIME_UNITS } from "./counters.ts"
import FrameChart from "./FrameChart.vue"
import { SYSTEMS, systemTime } from "./systems.ts"
import LineChart, { type Series } from "./LineChart.vue"
import type { Window } from "./view.ts"

const props = defineProps<{ frames: readonly PerfFrame[]; units: Record<string, string>; pinned: readonly string[] }>()
const emit = defineEmits<{ capture: [at: number]; span: [ms: number] }>()

const ZONE_LINES = 5
const COUNTER_LINES = 6
const SPAN = { least: 500, most: 15_000 }

// The window: always ending at the newest frame; Shift-wheel sets its width.
const span = ref(6000) // opens on six seconds: frame by frame at the panel's width; Shift-wheel goes out to fifteen
emit("span", span.value)
const win = computed<Window>(() => {
    const last = props.frames[props.frames.length - 1]
    const end = last ? last.end : 1
    return { startMs: end - span.value, endMs: end }
})

/**
 * Charts kept: key → their lines' names, in the order first given. Only ever
 * added to, by the frames as they come (over the last two seconds, so a new
 * chart or line finds its place within a moment of starting to matter).
 */
type Kept = { title: string; order: number; names: string[]; kind: "zones" | "counters"; threads: Set<string> }
const kept = ref(new Map<string, Kept>())
const kindOf = (thread: string) => thread.replace(/[-\s]?\d+$/, "") // "Emerge-3" with "Emerge-0"
const counterOrder = (id: string) => 100 + Math.max(0, GROUPS.findIndex((g) => g.id === id)) + (id === "other" ? 50 : 0)

// Whether this message changed what's kept: only then do the charts below
// remake (remade every message, every chart redrew every frame's worth).
let changed = false
function chart(key: string, make: () => Kept) {
    let k = kept.value.get(key)
    if (!k) {
        kept.value.set(key, (k = make()))
        changed = true
    }
    return k
}
function fill(k: Kept, ranked: [string, number][], room: number) {
    for (const [name] of ranked.sort((a, b) => b[1] - a[1])) {
        if (k.names.length >= room) return
        if (!k.names.includes(name)) {
            k.names.push(name)
            changed = true
        }
    }
}
function addThread(k: Kept, thread: string) {
    if (k.threads.has(thread)) return
    k.threads.add(thread)
    changed = true
}

watch(() => props.frames, (frames) => {
    const last = frames[frames.length - 1]
    if (!last) return
    const zoneSums = new Map<string, Map<string, number>>()
    const counterPeaks = new Map<string, Map<string, number>>()
    for (let i = frames.length - 1; i >= 0 && frames[i]!.end > last.end - 2000; i--) {
        const f = frames[i]!
        for (const [thread, parts] of Object.entries(f.threads)) {
            const kind = kindOf(thread)
            addThread(chart(`zones:${kind}`, () => ({ title: kind, order: 0, names: [], kind: "zones", threads: new Set() })), thread)
            let sums = zoneSums.get(kind)
            if (!sums) zoneSums.set(kind, (sums = new Map()))
            for (const [name, ms] of Object.entries(parts)) if (!IDLE.has(name)) sums.set(name, (sums.get(name) ?? 0) + ms)
        }
        for (const [name, v] of Object.entries(f.counters)) {
            if (TIME_UNITS.has(props.units[name] ?? "")) continue // a zone already, charted above
            const group = groupOf(name).id
            let peaks = counterPeaks.get(group)
            if (!peaks) counterPeaks.set(group, (peaks = new Map()))
            peaks.set(name, Math.max(peaks.get(name) ?? 0, v))
        }
    }
    for (const [kind, sums] of zoneSums) {
        const k = kept.value.get(`zones:${kind}`)
        if (k) fill(k, [...sums.entries()], ZONE_LINES)
    }
    for (const [group, peaks] of counterPeaks) {
        const title = group === "other" ? "Other" : (GROUPS.find((g) => g.id === group)?.title ?? group)
        fill(chart(`counters:${group}`, () => ({ title: title, order: counterOrder(group), names: [], kind: "counters", threads: new Set() })),
            [...peaks.entries()], COUNTER_LINES)
    }
    if (changed) kept.value = new Map(kept.value) // the charts below follow
    changed = false
}, { immediate: true })

const zoneValue = (k: Kept, name: string) => (f: PerfFrame) => {
    let sum = 0
    for (const t of k.threads) sum += f.threads[t]?.[name] ?? 0
    return sum
}
const counterValue = (name: string) => (f: PerfFrame) => f.counters[name] ?? Number.NaN
const charts = computed(() => [...kept.value.entries()]
    .filter(([, k]) => k.names.length)
    .sort(([a, ka], [b, kb]) => ka.order - kb.order || (ka.kind === "zones" ? threadOrder(ka.title, kb.title) : a.localeCompare(b)))
    .map(([key, k]) => ({
        key: key,
        title: k.kind === "zones" && k.threads.size > 1 ? `${k.title} ×${k.threads.size}` : k.title,
        counters: k.kind === "counters",
        series: k.names.map((name): Series => k.kind === "zones"
            ? { name: name, color: colorOf(name), value: zoneValue(k, name) }
            : { name: name, color: colorOf(name), value: counterValue(name), unit: props.units[name] ?? "" }),
    })))
const pinnedSeries = computed<Series[]>(() => props.pinned.map((name) => ({ name: name, color: colorOf(name), value: counterValue(name), unit: props.units[name] ?? "" })))
/**
 * A chart a system (systems.ts), once it's shown up: one that never runs here
 * isn't drawn. Only the newest frames are looked at, and once shown it stays.
 */
const shownSystems = ref(new Set<string>())
watch(() => props.frames, (frames) => {
    let added = false
    for (let i = Math.max(0, frames.length - 30); i < frames.length; i++)
        for (const sys of SYSTEMS)
            if (!shownSystems.value.has(sys.title) && systemTime(sys, frames[i]!) > 0) {
                shownSystems.value.add(sys.title)
                added = true
            }
    if (added) shownSystems.value = new Set(shownSystems.value)
}, { immediate: true })
const systemCharts = computed(() => SYSTEMS
    .filter((sys) => shownSystems.value.has(sys.title))
    .map((sys) => ({ title: sys.title, series: [{ name: "time", color: colorOf(sys.title), value: (f: PerfFrame) => systemTime(sys, f) }] as Series[] })))

const box = ref<HTMLElement | null>(null)
const cursor = ref<number | null>(null)
const msAt = (event: MouseEvent) => {
    const rect = box.value?.getBoundingClientRect()
    if (!rect) return null
    return win.value.startMs + ((event.clientX - rect.left) / rect.width) * (win.value.endMs - win.value.startMs)
}
function onClick(event: MouseEvent) {
    const at = msAt(event)
    if (at !== null) emit("capture", at)
}
// Shift-wheel sets the span; the wheel alone scrolls the charts.
function onWheel(event: WheelEvent) {
    if (!event.shiftKey) return
    event.preventDefault()
    span.value = Math.min(SPAN.most, Math.max(SPAN.least, span.value * Math.exp(wheelDelta(event) * 0.0015)))
    emit("span", span.value)
}
</script>

<template>
    <div v-if="frames.length" ref="box" class="live" @mousemove="(e: MouseEvent) => (cursor = msAt(e))" @mouseleave="cursor = null"
        @click="onClick" @wheel="onWheel">
        <FrameChart :frames="frames" :window="win" :cursor="cursor" :height="140" />
        <LineChart v-for="s in systemCharts" :key="s.title" :title="s.title" :frames="frames" :window="win" :series="s.series" :height="72"
            :cursor="cursor" bounds />
        <LineChart v-if="pinnedSeries.length" title="Pinned" :frames="frames" :window="win" :series="pinnedSeries" :height="80"
            :cursor="cursor" />
        <LineChart v-for="c in charts" :key="c.key" :title="c.title" :frames="frames" :window="win" :series="c.series" :height="70"
            :cursor="cursor" />
    </div>
    <p v-else class="waiting">Waiting for frames from the engine…</p>
</template>

<style scoped>
.live { height: 100%; min-height: 0; overflow-y: auto; cursor: crosshair; }
.waiting { height: 100%; display: grid; place-items: center; margin: 0; color: var(--dt-faint); }
</style>
