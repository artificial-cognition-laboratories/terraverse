<script setup lang="ts">
/**
 * One chart of the Live tab: a few series over the shared window, as lines,
 * each pixel column drawing the most of its frames (a one-frame spike shows at
 * any width). Its title and each line's value sit in the corner: the newest
 * frame's, or the one under the cursor.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import type { Window } from "./view.ts"

export type Series = { name: string; color: string; value: (frame: PerfFrame) => number; bold?: boolean }

const props = defineProps<{
    title: string
    frames: readonly PerfFrame[]
    window: Window
    series: readonly Series[]
    height: number
    /** ms lines to draw across: the frame budgets, on the frame chart. */
    budgets?: readonly { ms: number; label: string }[]
    cursor: number | null
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const width = ref(0)

/** The frame the corner reads: under the cursor, or the newest in view. */
const reading = computed(() => {
    const list = props.frames
    if (props.cursor !== null) {
        const at = props.cursor
        return list.find((f) => f.end >= at) ?? list[list.length - 1] ?? null
    }
    for (let i = list.length - 1; i >= 0; i--) if (list[i]!.end <= props.window.endMs) return list[i]!
    return null
})
const fmt = (ms: number) => (ms >= 10 ? ms.toFixed(1) : ms.toFixed(2))

function draw() {
    const el = canvas.value
    if (!el || width.value === 0) return
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
    const cols = Math.max(1, Math.floor(width.value))
    const columns = props.series.map(() => new Float32Array(cols).fill(Number.NaN))
    for (const f of props.frames) {
        if (f.end < startMs || f.start > endMs) continue
        const x = Math.min(cols - 1, Math.max(0, Math.floor(((f.end - startMs) / span) * width.value)))
        props.series.forEach((s, k) => {
            const col = columns[k]!
            const v = s.value(f)
            const was = col[x]!
            col[x] = Number.isNaN(was) ? v : Math.max(was, v)
        })
    }
    let top = 0
    for (const col of columns) for (const v of col) if (!Number.isNaN(v)) top = Math.max(top, v)
    for (const b of props.budgets ?? []) top = Math.max(top, b.ms)
    top = Math.max(top * 1.15, 0.05)
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
        const col = columns[k]!
        g.beginPath()
        let pen = false
        col.forEach((v, x) => {
            if (Number.isNaN(v)) return
            if (pen) g.lineTo(x + 0.5, y(v))
            else g.moveTo(x + 0.5, y(v))
            pen = true
        })
        if (!pen) return
        g.strokeStyle = s.color
        g.lineWidth = s.bold ? 1.5 : 1.2
        g.lineJoin = "round"
        g.stroke()
    })

    if (props.cursor !== null && props.cursor >= startMs && props.cursor <= endMs) {
        g.fillStyle = "rgb(255 255 255 / 0.35)"
        g.fillRect(Math.round(((props.cursor - startMs) / span) * width.value), 0, 1, props.height)
    }
}

let pending: number | null = null
function redraw() {
    if (pending !== null) return
    pending = requestAnimationFrame(() => {
        pending = null
        draw()
    })
}
watch(() => [props.frames, props.series, props.window.startMs, props.window.endMs, props.cursor, props.height], redraw)

let observer: ResizeObserver | null = null
onMounted(() => {
    const el = canvas.value?.parentElement
    if (!el) throw new Error("perf: a chart isn't mounted")
    observer = new ResizeObserver(() => {
        width.value = el.clientWidth
        redraw()
    })
    observer.observe(el)
})
onUnmounted(() => {
    observer?.disconnect()
    if (pending !== null) cancelAnimationFrame(pending)
})
</script>

<template>
    <div class="chart" :style="{ height: `${height}px` }">
        <canvas ref="canvas" />
        <div class="read">
            <b>{{ title }}</b>
            <span v-for="s in series" :key="s.name"><i :style="{ background: s.color }" />{{ s.name }}
                <em>{{ reading ? fmt(s.value(reading)) : "–" }}</em></span>
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
