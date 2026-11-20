<script setup lang="ts">
/**
 * The frame chart, as Luanti's: one line, the target (60 fps) across the
 * middle, read two ways. A frame that made its target is above it, green, by
 * the time it had to spare (what it slept), filled to the middle. One that
 * didn't is below it, red, by its fps. Both halves scale to the window: the
 * most spare at the top, the least fps at the bottom. Each pixel column is
 * its frames' mean, its worst faint behind, so one hitch isn't averaged away.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import { forget, markStale } from "./drawLoop.ts"
import type { Window } from "./view.ts"

const props = defineProps<{ frames: readonly PerfFrame[]; window: Window; cursor: number | null; height: number }>()

const TARGET_FPS = 60
const TARGET_MS = 1000 / TARGET_FPS
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
    // a column per device pixel's worth of time: its frames' mean work (the
    // typical frame) and its most (a hitch, faint behind)
    const msPerPixel = span / Math.max(width.value * dpr, 1)
    const columns: { x: number; work: number; mostWork: number }[] = []
    let group = Number.NaN
    let n = 0
    for (let k = firstFrom(startMs); k < props.frames.length; k++) {
        const f = props.frames[k]!
        if (f.start > endMs) break
        const at = Math.floor(f.end / msPerPixel)
        const x = ((f.end - startMs) / span) * width.value
        const work = workOf(f)
        const last = columns[columns.length - 1]
        if (at !== group || !last) {
            columns.push({ x: x, work: work, mostWork: work })
            group = at
            n = 1
        } else {
            n++
            last.x = x
            last.work += (work - last.work) / n
            last.mostWork = Math.max(last.mostWork, work)
        }
    }
    const top = 18 // the reading's row
    const half = (props.height - top) / 2
    const mid = top + half
    // Each half scaled to the window, as the other charts: the top is the
    // most spare time in it, the bottom its least fps (the worst frames', so
    // a hitch never runs off the chart)
    let mostSpare = 0
    let leastFps = TARGET_FPS
    for (const c of columns) {
        mostSpare = Math.max(mostSpare, TARGET_MS - c.work)
        leastFps = Math.min(leastFps, 1000 / Math.max(c.mostWork, 1e-3))
    }
    mostSpare = Math.max(mostSpare, TARGET_MS * 0.05) // (a window with none still has a top half)
    leastFps = Math.min(leastFps, TARGET_FPS * 0.95)
    // One line, read two ways: a frame that made its target sits above the
    // middle by the time it had to spare (target − work, what it slept); one
    // that didn't sits below it by its fps, the target at the middle
    const yOf = (work: number) => {
        if (work <= TARGET_MS) return mid - Math.min(1, (TARGET_MS - work) / mostSpare) * half
        const fps = 1000 / work
        return mid + Math.min(1, (TARGET_FPS - fps) / (TARGET_FPS - leastFps)) * half
    }
    // the spare time: solid green from the middle up to the line
    g.fillStyle = "rgb(76 175 106 / 0.45)"
    for (let i = 0; i < columns.length; i++) {
        const c = columns[i]!
        const y = yOf(c.work)
        if (y >= mid) continue
        const x0 = i ? columns[i - 1]!.x : c.x - 1
        g.fillRect(x0, y, Math.max(1 / dpr, c.x - x0), mid - y)
    }
    // the line, green above the middle and red below (clipped each side)
    const line = (pick: (c: (typeof columns)[number]) => number, alpha: number) => {
        for (const [y0, y1, color] of [
            [0, mid, `rgb(76 175 106 / ${alpha})`],
            [mid, props.height, `rgb(224 82 79 / ${alpha})`],
        ] as const) {
            g.save()
            g.beginPath()
            g.rect(0, y0, width.value, y1 - y0)
            g.clip()
            g.beginPath()
            columns.forEach((c, i) => {
                if (i) g.lineTo(c.x, yOf(pick(c)))
                else g.moveTo(c.x, yOf(pick(c)))
            })
            g.strokeStyle = color
            g.lineWidth = 1.2
            g.lineJoin = "round"
            g.stroke()
            g.restore()
        }
    }
    line((c) => c.mostWork, 0.35) // the column's worst frame, faint
    line((c) => c.work, 1)
    const across = Math.round(mid) + 0.5
    g.strokeStyle = "rgb(255 255 255 / 0.5)"
    g.setLineDash([3, 3])
    g.beginPath()
    g.moveTo(0, across)
    g.lineTo(width.value, across)
    g.stroke()
    g.setLineDash([])
    // the scale at the right: all spare at the top, the target, the least fps
    g.font = "9px ui-monospace, monospace"
    g.textAlign = "right"
    g.fillStyle = "rgb(255 255 255 / 0.45)"
    g.textBaseline = "top"
    g.fillText(`${us(mostSpare)} sleep`, width.value - 4, top + 1)
    g.textBaseline = "bottom"
    g.fillText(`${TARGET_FPS} fps`, width.value - 4, across - 1)
    g.fillText(`${leastFps.toFixed(leastFps < 10 ? 1 : 0)} fps`, width.value - 4, props.height - 2)
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
