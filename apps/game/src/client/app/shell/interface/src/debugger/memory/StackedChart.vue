<script setup lang="ts">
/**
 * A whole and its parts through time: each band stacked on the last, the top
 * of the chart the whole (the machine's memory, the heap, the card), so the
 * chart reads as how full it is and who it's full of. A line may run over the
 * bands (everything on the machine in use, the game's bands under it), and a
 * strip may hang below the floor, red, scaled to its own most (swap: the
 * danger side of the same question, as the frame chart's overruns hang below
 * its target). The corner reads each band at the cursor, or now: its size and
 * share of the whole.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import { formatBytes } from "../perf/counters.ts"
import { forget, markStale } from "../perf/drawLoop.ts"
import type { Window } from "../perf/view.ts"
import { COLORS, type Band } from "./figures.ts"
import type { Sampled } from "./store.ts"

const props = defineProps<{
    title: string
    samples: readonly Sampled[]
    window: Window
    bands: readonly Band[]
    /** The whole the bands are parts of: the chart's top. */
    whole: (s: Sampled) => number
    /** A line over the bands, on the same scale. */
    line?: { name: string; value: (s: Sampled) => number }
    /** A strip under the floor, red, on its own scale. */
    below?: { name: string; value: (s: Sampled) => number }
    /** Said instead of drawing when the whole is unknown (0): why. */
    unknown: string
    cursor: number | null
    marks: readonly { ms: number; label: string }[]
    height: number
}>()

const HEAD = 16
const BELOW = 22
const canvas = ref<HTMLCanvasElement | null>(null)
const width = ref(0)
let visible = false

/** The sample at the cursor, or the newest in the window. */
const reading = computed(() => {
    const at = props.cursor ?? props.window.endMs
    for (let i = props.samples.length - 1; i >= 0; i--) if (props.samples[i]!.end <= at + 100) return props.samples[i]!
    return null
})
const known = computed(() => (reading.value ? props.whole(reading.value) > 0 : true))
const parts = computed(() => {
    const s = reading.value
    if (!s) return []
    const whole = props.whole(s)
    return props.bands.map((b) => {
        const v = b.value(s)
        return { name: b.name, color: b.color, text: formatBytes(v), share: whole > 0 ? `${Math.round((v / whole) * 100)}%` : "" }
    })
})
const lineText = computed(() => (props.line && reading.value ? formatBytes(props.line.value(reading.value)) : ""))
const belowText = computed(() => (props.below && reading.value ? formatBytes(props.below.value(reading.value)) : ""))
const wholeText = computed(() => (reading.value ? formatBytes(props.whole(reading.value)) : ""))

function firstFrom(ms: number) {
    let lo = 0
    let hi = props.samples.length
    while (lo < hi) {
        const mid = (lo + hi) >> 1
        if (props.samples[mid]!.end < ms) lo = mid + 1
        else hi = mid
    }
    return lo
}

function draw() {
    const el = canvas.value
    if (!el || width.value === 0 || !visible) return
    const dpr = window.devicePixelRatio || 1
    const w = Math.round(width.value * dpr)
    const h = Math.round(props.height * dpr)
    if (el.width !== w || el.height !== h) {
        el.width = w
        el.height = h
    }
    const g = el.getContext("2d")
    if (!g) throw new Error("memory: no 2d context for a chart")
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, width.value, props.height)
    if (!known.value) return

    const { startMs, endMs } = props.window
    const span = Math.max(endMs - startMs, 1e-6)
    const xOf = (ms: number) => ((ms - startMs) / span) * width.value
    const floor = props.height - (props.below ? BELOW : 2)
    const yOf = (share: number) => floor - Math.min(Math.max(share, 0), 1) * (floor - HEAD)

    // a point a device pixel: the newest sample in each, fixed in time so the shape doesn't shimmer as it slides
    const msPerPixel = span / Math.max(width.value * dpr, 1)
    const picked: Sampled[] = []
    let group = Number.NaN
    for (let n = firstFrom(startMs - msPerPixel * 4); n < props.samples.length; n++) {
        const s = props.samples[n]!
        if (s.start > endMs) break
        const at = Math.floor(s.end / msPerPixel)
        if (at === group) picked[picked.length - 1] = s
        else picked.push(s)
        group = at
    }
    if (!picked.length) return

    // each band's top, as a share of the whole, sample by sample
    const tops = picked.map((s) => {
        const whole = props.whole(s)
        let cum = 0
        return props.bands.map((b) => (cum += whole > 0 ? b.value(s) / whole : 0))
    })
    props.bands.forEach((b, k) => {
        g.beginPath()
        picked.forEach((s, i) => (i ? g.lineTo(xOf(s.end), yOf(tops[i]![k]!)) : g.moveTo(xOf(s.end), yOf(tops[i]![k]!))))
        for (let i = picked.length - 1; i >= 0; i--) g.lineTo(xOf(picked[i]!.end), yOf(k ? tops[i]![k - 1]! : 0))
        g.closePath()
        g.fillStyle = b.color
        g.globalAlpha = 0.75
        g.fill()
        g.globalAlpha = 1
    })

    if (props.line) {
        const line = props.line
        g.beginPath()
        picked.forEach((s, i) => {
            const whole = props.whole(s)
            const y = yOf(whole > 0 ? line.value(s) / whole : 0)
            if (i) g.lineTo(xOf(s.end), y)
            else g.moveTo(xOf(s.end), y)
        })
        g.strokeStyle = "rgb(255 255 255 / 0.7)"
        g.lineWidth = 1.2
        g.setLineDash([4, 3])
        g.stroke()
        g.setLineDash([])
    }

    if (props.below) {
        const below = props.below
        const most = Math.max(1, ...picked.map((s) => below.value(s)))
        g.fillStyle = "rgb(255 255 255 / 0.06)"
        g.fillRect(0, floor, width.value, 1)
        g.beginPath()
        g.moveTo(xOf(picked[0]!.end), floor + 1)
        for (const s of picked) g.lineTo(xOf(s.end), floor + 1 + (below.value(s) / most) * (BELOW - 3))
        g.lineTo(xOf(picked[picked.length - 1]!.end), floor + 1)
        g.closePath()
        g.fillStyle = COLORS.swap
        g.globalAlpha = 0.7
        g.fill()
        g.globalAlpha = 1
    }

    g.font = "9px system-ui, sans-serif"
    g.textBaseline = "bottom"
    for (const m of props.marks) {
        if (m.ms < startMs || m.ms > endMs) continue
        const mx = Math.round(xOf(m.ms)) + 0.5
        g.strokeStyle = "rgb(255 210 120 / 0.55)"
        g.setLineDash([2, 3])
        g.beginPath()
        g.moveTo(mx, HEAD)
        g.lineTo(mx, props.height)
        g.stroke()
        g.setLineDash([])
        g.fillStyle = "rgb(255 210 120 / 0.8)"
        g.fillText(m.label, mx + 3, floor - 2)
    }
    if (props.cursor !== null && props.cursor >= startMs && props.cursor <= endMs) {
        g.fillStyle = "rgb(255 255 255 / 0.35)"
        g.fillRect(Math.round(xOf(props.cursor)), 0, 1, props.height)
    }
}

const redraw = () => markStale(draw)
watch(() => [props.samples, props.bands, props.window.startMs, props.window.endMs, props.cursor, props.height, props.marks], redraw)

let observer: ResizeObserver | null = null
let seen: IntersectionObserver | null = null
onMounted(() => {
    const el = canvas.value?.parentElement
    if (!el) throw new Error("memory: a chart isn't mounted")
    observer = new ResizeObserver(() => {
        width.value = el.clientWidth
        redraw()
    })
    observer.observe(el)
    seen = new IntersectionObserver((entries) => {
        visible = entries.some((e) => e.isIntersecting)
        if (visible) redraw()
    })
    seen.observe(el)
})
onUnmounted(() => {
    observer?.disconnect()
    seen?.disconnect()
    forget(draw)
})
</script>

<template>
    <div class="chart" :style="{ height: `${height}px` }">
        <canvas ref="canvas" />
        <div class="read">
            <b>{{ title }}</b>
            <template v-if="known">
                <span v-for="p in parts" :key="p.name"><i :style="{ background: p.color }" />{{ p.name }} <em>{{ p.text }}</em><small>{{ p.share }}</small></span>
                <span v-if="line"><i class="dash" />{{ line.name }} <em>{{ lineText }}</em></span>
                <span v-if="below"><i :style="{ background: COLORS.swap }" />{{ below.name }} <em>{{ belowText }}</em></span>
            </template>
        </div>
        <span v-if="known" class="whole">of {{ wholeText }}</span>
        <p v-else class="unknown">{{ unknown }}</p>
    </div>
</template>

<style scoped>
.chart { position: relative; min-width: 0; border-bottom: 1px solid var(--dt-line); }
canvas { display: block; width: 100%; height: 100%; }
.read {
    position: absolute; top: 2px; left: 6px; right: 70px; display: flex; flex-wrap: wrap; gap: 0 10px; pointer-events: none;
    font-size: 11px; color: var(--dt-dim); white-space: nowrap; overflow: hidden; max-height: 15px;
}
.read b { color: var(--dt-ink); font-weight: 600; }
.read span { display: flex; align-items: center; gap: 4px; }
.read i { width: 8px; height: 8px; border-radius: 1px; }
.read i.dash { height: 0; border-top: 1px dashed rgb(255 255 255 / 0.7); }
.read em { color: var(--dt-ink); font-style: normal; font-family: ui-monospace, monospace; }
.read small { color: var(--dt-faint); font-family: ui-monospace, monospace; font-size: 10px; }
.whole { position: absolute; top: 3px; right: 6px; color: var(--dt-faint); font: 9px ui-monospace, monospace; pointer-events: none; }
.unknown { position: absolute; inset: 16px 8px 0; margin: 0; display: flex; align-items: center; justify-content: center; color: var(--dt-faint); font-size: 11px; text-align: center; }
</style>
