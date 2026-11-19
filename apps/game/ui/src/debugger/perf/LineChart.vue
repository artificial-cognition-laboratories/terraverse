<script setup lang="ts">
/**
 * One chart of the Live tab: a few series over the shared window, as lines,
 * each pixel column drawing the most of its frames (a one-frame spike shows at
 * any width). Its title and each line's value sit in the corner, with its
 * unit (a zone's time in µs under a millisecond, ms over): the newest
 * frame's, or the one under the cursor. A series with nothing for a frame
 * (a counter that wasn't reported) leaves a gap there.
 *
 * Drawn only while on screen, and only the frames in the window: the panel
 * shares the game's frame, so every chart redrawn every message, all of the
 * kept frames each, was the game's time too.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import { formatValue } from "./counters.ts"
import { forget, markStale } from "./drawLoop.ts"
import type { Window } from "./view.ts"

/** `unit`: a counter's, as the engine names it ("#", "%"); none: a time in ms. */
export type Series = { name: string; color: string; value: (frame: PerfFrame) => number; bold?: boolean; unit?: string }

const props = defineProps<{
    title: string
    frames: readonly PerfFrame[]
    window: Window
    series: readonly Series[]
    height: number
    /** ms lines to draw across: the frame budgets, on the frame chart. */
    budgets?: readonly { ms: number; label: string }[]
    cursor: number | null
    /**
     * As Luanti's graphs: one series on a scale from zero to its most in the
     * window, that most at the top right and its least (the work it does
     * every frame, whatever happens) at the bottom right.
     */
    bounds?: boolean
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const width = ref(0)

/**
 * What the corner reads for each series: its value at the cursor, or at the
 * newest frame in view; for a counter not reported every frame, the last it was.
 */
const readings = computed(() => {
    const list = props.frames
    const at = props.cursor ?? props.window.endMs
    let i = list.length - 1
    while (i >= 0 && list[i]!.start > at) i--
    return props.series.map((s) => {
        for (let k = i; k >= 0 && k > i - 120; k--) {
            const v = s.value(list[k]!)
            if (!Number.isNaN(v)) return v
        }
        return Number.NaN
    })
})
/** A time (ms) in the unit that reads best: µs under a millisecond. */
function time(ms: number) {
    if (ms === 0) return "0"
    if (ms < 1) return `${Math.round(ms * 1000)} µs`
    return `${ms >= 10 ? ms.toFixed(1) : ms.toFixed(2)} ms`
}
const fmt = (s: Series, v: number) => (Number.isNaN(v) ? "–" : s.unit === undefined ? time(v) : formatValue(v, s.unit))

/** The first frame that could show in the window (frames are in order). */
function firstFrom(ms: number) {
    const list = props.frames
    let lo = 0
    let hi = list.length
    while (lo < hi) {
        const mid = (lo + hi) >> 1
        if (list[mid]!.end < ms) lo = mid + 1
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
    if (!g) throw new Error("perf: no 2d context for a chart")
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, width.value, props.height)

    const { startMs, endMs } = props.window
    const span = Math.max(endMs - startMs, 1e-6)
    const xOf = (ms: number) => ((ms - startMs) / span) * width.value

    // Points, per series. Grouped a pixel's worth of time at a time, the most
    // in each group, with the groups cut on absolute time, never on the
    // window: as the window slides, the same frames stay together and the line
    // moves whole, rather than regrouping (and changing shape) every update.
    // Zoomed in past a frame a pixel, each frame is its own point.
    const msPerPixel = span / Math.max(width.value * dpr, 1) // a group a device pixel
    // A group's point sits at its newest frame's end: fixed in time (so it
    // doesn't jitter), and the newest of all exactly at the right border. A
    // few groups past the left edge are drawn too, and cut by it, so the line
    // runs in from beyond the border rather than starting short of it.
    const points = props.series.map(() => [] as [number, number][])
    let least = Number.POSITIVE_INFINITY // the first series', frame by frame, in the window
    let most_ = 0
    let group = Number.NaN
    let most: number[] = []
    let groupEnd = 0
    const flush = () => {
        if (Number.isNaN(group)) return
        most.forEach((v, k) => {
            if (!Number.isNaN(v)) points[k]!.push([xOf(groupEnd), v])
        })
    }
    for (let n = firstFrom(startMs - msPerPixel * 4); n < props.frames.length; n++) {
        const f = props.frames[n]!
        if (f.start > endMs) break
        const at = Math.floor(f.end / msPerPixel)
        if (at !== group) {
            flush()
            group = at
            most = props.series.map(() => Number.NaN)
        }
        groupEnd = f.end
        props.series.forEach((s, k) => {
            const v = s.value(f)
            if (Number.isNaN(v)) return // not reported that frame: a gap, not a zero
            if (k === 0 && f.start >= startMs) {
                least = Math.min(least, v)
                most_ = Math.max(most_, v)
            }
            const was = most[k]!
            most[k] = Number.isNaN(was) ? v : Math.max(was, v)
        })
    }
    flush()
    let top = 0
    for (const list of points) for (const [, v] of list) top = Math.max(top, v)
    for (const b of props.budgets ?? []) top = Math.max(top, b.ms)
    top = props.bounds ? Math.max(most_, 1e-6) : Math.max(top * 1.15, 0.05)
    const pad = 3
    const y = (v: number) => props.height - pad - (v / top) * (props.height - pad * 2 - 12)

    g.font = "10px system-ui, sans-serif"
    g.textBaseline = "bottom"
    for (const b of props.budgets ?? []) {
        const by = Math.round(y(b.ms)) + 0.5
        g.strokeStyle = "rgb(255 255 255 / 0.12)"
        g.setLineDash([3, 3])
        g.beginPath()
        g.moveTo(0, by)
        g.lineTo(width.value, by)
        g.stroke()
        g.setLineDash([])
        g.fillStyle = "rgb(255 255 255 / 0.3)"
        g.fillText(b.label, 4, by - 1)
    }

    props.series.forEach((s, k) => {
        const list = points[k]!
        if (!list.length) return
        g.beginPath()
        list.forEach(([x, v], i) => (i ? g.lineTo(x, y(v)) : g.moveTo(x, y(v))))
        g.strokeStyle = s.color
        g.lineWidth = s.bold ? 1.5 : 1.2
        g.lineJoin = "round"
        g.stroke()
    })

    if (props.bounds && Number.isFinite(least)) {
        // the scale's ends, fixed in the corners (riding the line, they moved as it did)
        const unit = props.series[0]?.unit
        const say = (v: number) => (unit === undefined ? time(v) : formatValue(v, unit))
        g.font = "9px ui-monospace, monospace"
        g.textAlign = "right"
        g.fillStyle = "rgb(255 255 255 / 0.45)"
        g.textBaseline = "top"
        g.fillText(say(most_), width.value - 4, 3)
        g.textBaseline = "bottom"
        g.fillText(say(least), width.value - 4, props.height - 2)
        g.textAlign = "left"
    }
    if (props.cursor !== null && props.cursor >= startMs && props.cursor <= endMs) {
        g.fillStyle = "rgb(255 255 255 / 0.35)"
        g.fillRect(Math.round(((props.cursor - startMs) / span) * width.value), 0, 1, props.height)
    }
}

// drawn with the rest of the panel's charts (drawLoop.ts)
const redraw = () => markStale(draw)
watch(() => [props.frames, props.series, props.window.startMs, props.window.endMs, props.cursor, props.height], redraw)

let observer: ResizeObserver | null = null
let seen: IntersectionObserver | null = null
let visible = false
onMounted(() => {
    const el = canvas.value?.parentElement
    if (!el) throw new Error("perf: a chart isn't mounted")
    observer = new ResizeObserver(() => {
        width.value = el.clientWidth
        redraw()
    })
    observer.observe(el)
    // scrolled out of the panel, it isn't drawn; back in, it catches up at once
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
            <span v-for="(s, k) in series" :key="s.name"><i :style="{ background: s.color }" />{{ s.name }}
                <em>{{ fmt(s, readings[k] ?? Number.NaN) }}</em></span>
        </div>
    </div>
</template>

<style scoped>
.chart { position: relative; min-width: 0; border-bottom: 1px solid var(--dt-line); }
canvas { display: block; width: 100%; height: 100%; }
.read {
    position: absolute; top: 3px; left: 6px; right: 6px; display: flex; flex-wrap: wrap; gap: 0 10px; pointer-events: none;
    font-size: 11px; color: var(--dt-dim); white-space: nowrap; overflow: hidden; max-height: 16px;
}
.read b { color: var(--dt-ink); font-weight: 600; }
.read span { display: flex; align-items: center; gap: 4px; }
.read i { width: 8px; height: 2px; border-radius: 1px; }
.read em { color: var(--dt-ink); font-style: normal; font-family: ui-monospace, monospace; }
</style>
