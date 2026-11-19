<script setup lang="ts">
/**
 * The frame chart, as Luanti's: frame time on a scale with the target (60 fps)
 * across the middle, upside down: frames that make it fill green above it (the
 * spare time), frames over it are a red line below. How close each frame came to its
 * budget, at a glance (a column's mean; under the line its slowest frame faint behind),
 * and the reading says how much was work and how much
 * spare (the main thread's Wait). A pixel column shows its slowest frame;
 * past twice the target it's cut at the bottom.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import { forget, markStale } from "./drawLoop.ts"
import type { Window } from "./view.ts"

const props = defineProps<{ frames: readonly PerfFrame[]; window: Window; cursor: number | null; height: number }>()

const TARGET_MS = 1000 / 60
const MAIN = "Main"
const WAIT = "Wait"
const GOOD = "#4caf6a"
const OVER = "#e0524f"
const SPARE = "rgb(255 255 255 / 0.25)"

const waitOf = (f: PerfFrame) => f.threads[MAIN]?.[WAIT] ?? 0
const workOf = (f: PerfFrame) => Math.max(0, f.end - f.start - waitOf(f))

/** The frame at the cursor (or the newest): its time, work and spare. */
const reading = computed(() => {
    const at = props.cursor ?? props.window.endMs
    for (let i = props.frames.length - 1; i >= 0; i--) {
        const f = props.frames[i]!
        if (f.start <= at) return { frame: f.end - f.start, work: workOf(f), spare: waitOf(f) }
    }
    return null
})
const us = (ms: number) => `${Math.round(ms * 1000).toLocaleString()} µs`

function firstFrom(ms: number) {
    let lo = 0
    let hi = props.frames.length
    while (lo < hi) {
        const mid = (lo + hi) >> 1
        if (props.frames[mid]!.end < ms) lo = mid + 1
        else hi = mid
    }
    return lo
}

const canvas = ref<HTMLCanvasElement | null>(null)
const width = ref(0)
let visible = false

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
    if (!g) throw new Error("perf: no 2d context for the frame chart")
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, width.value, props.height)

    const { startMs, endMs } = props.window
    const span = Math.max(endMs - startMs, 1e-6)
    // a column per device pixel's worth of time: its frames' mean (the typical
    // frame) and its slowest (a hitch, faint). The slowest alone painted every
    // column with one hitch in it red, as if the game never made its target.
    const msPerPixel = span / Math.max(width.value * dpr, 1)
    const columns: { x: number; mean: number; most: number }[] = []
    let group = Number.NaN
    let sum = 0
    let n = 0
    for (let k = firstFrom(startMs); k < props.frames.length; k++) {
        const f = props.frames[k]!
        if (f.start > endMs) break
        const at = Math.floor(f.end / msPerPixel)
        const x = ((f.end - startMs) / span) * width.value
        const ms = f.end - f.start
        const last = columns[columns.length - 1]
        if (at !== group || !last) {
            columns.push({ x: x, mean: ms, most: ms })
            group = at
            sum = ms
            n = 1
        } else {
            sum += ms
            n++
            last.x = x
            last.mean = sum / n
            last.most = Math.max(last.most, ms)
        }
    }
    const top = 18 // the reading's row
    const full = TARGET_MS * 2 // the target across the middle
    const scale = (props.height - top) / full
    // upside down, as spare time reads: a quick frame high, a slow one low,
    // so the green line rides the top and a frame over the target drops red below it
    const yOf = (ms: number) => top + Math.min(ms, full) * scale
    const target = yOf(TARGET_MS)
    // Over the line (frames that made the target, their spare time) solid
    // green; under it (frames over the target) a red line only, the column's
    // slowest frame a faint one behind it.
    g.fillStyle = GOOD
    for (let i = 0; i < columns.length; i++) {
        const c = columns[i]!
        if (c.mean > TARGET_MS) continue
        const x0 = i ? columns[i - 1]!.x : c.x - 1
        g.fillRect(x0, yOf(c.mean), Math.max(1 / dpr, c.x - x0), target - yOf(c.mean))
    }
    const under = (pick: (c: (typeof columns)[number]) => number, color: string) => {
        g.beginPath()
        columns.forEach((c, i) => {
            const y = yOf(Math.max(pick(c), TARGET_MS))
            if (i) g.lineTo(c.x, y)
            else g.moveTo(c.x, y)
        })
        g.strokeStyle = color
        g.lineWidth = 1.2
        g.lineJoin = "round"
        g.stroke()
    }
    under((c) => c.most, "rgb(224 82 79 / 0.3)")
    under((c) => c.mean, OVER)
    const mid = Math.round(target) + 0.5
    g.strokeStyle = "rgb(255 255 255 / 0.5)"
    g.setLineDash([3, 3])
    g.beginPath()
    g.moveTo(0, mid)
    g.lineTo(width.value, mid)
    g.stroke()
    g.setLineDash([])
    // the scale at the right, as the other charts': the most spare at the top
    // (a frame of no work), the target in the middle, as far over at the bottom
    g.font = "9px ui-monospace, monospace"
    g.textAlign = "right"
    g.fillStyle = "rgb(255 255 255 / 0.45)"
    g.textBaseline = "top"
    g.fillText("16,667 µs spare", width.value - 4, top + 1)
    g.textBaseline = "bottom"
    g.fillText("60 fps", width.value - 4, mid - 1)
    g.fillText("16,667 µs over", width.value - 4, props.height - 2)
    g.textAlign = "left"
    if (props.cursor !== null && props.cursor >= startMs && props.cursor <= endMs) {
        g.fillStyle = "rgb(255 255 255 / 0.35)"
        g.fillRect(Math.round(((props.cursor - startMs) / span) * width.value), 0, 1, props.height)
    }
}
const redraw = () => markStale(draw)
watch(() => [props.frames, props.window.startMs, props.window.endMs, props.cursor, props.height], redraw)

let resize: ResizeObserver | null = null
let seen: IntersectionObserver | null = null
onMounted(() => {
    const el = canvas.value?.parentElement
    if (!el) throw new Error("perf: the frame chart isn't mounted")
    resize = new ResizeObserver(() => {
        width.value = el.clientWidth
        redraw()
    })
    resize.observe(el)
    seen = new IntersectionObserver((entries) => {
        visible = entries.some((e) => e.isIntersecting)
        if (visible) redraw()
    })
    seen.observe(el)
})
onUnmounted(() => {
    resize?.disconnect()
    seen?.disconnect()
    forget(draw)
})
</script>

<template>
    <div class="frame" :style="{ height: `${height}px` }">
        <canvas ref="canvas" />
        <div class="read">
            <b>Frame</b>
            <template v-if="reading">
                <span><i :style="{ background: reading.frame > TARGET_MS ? OVER : GOOD }" />work <em>{{ us(reading.work) }}</em></span>
                <span><i :style="{ background: SPARE }" />spare <em>{{ us(reading.spare) }}</em></span>
                <span>frame <em>{{ us(reading.frame) }}</em></span>
                <span>fps <em>{{ (1000 / Math.max(reading.frame, 0.001)).toFixed(0) }}</em></span>
            </template>
        </div>
    </div>
</template>

<style scoped>
.frame { position: relative; min-width: 0; border-bottom: 1px solid var(--dt-line); }
canvas { display: block; width: 100%; height: 100%; }
.read {
    position: absolute; top: 3px; left: 6px; right: 6px; display: flex; flex-wrap: wrap; gap: 0 10px; pointer-events: none;
    font-size: 11px; color: var(--dt-dim); white-space: nowrap; overflow: hidden; max-height: 16px;
}
.read b { color: var(--dt-ink); font-weight: 600; }
.read span { display: flex; align-items: center; gap: 4px; }
.read i { width: 8px; height: 8px; }
.read em { color: var(--dt-ink); font-style: normal; font-family: ui-monospace, monospace; }
</style>
