<script setup lang="ts">
/**
 * Every frame the page has, always, in a strip above the flame graph: frame
 * time as a line, and the chosen zones' time per frame as thinner lines
 * behind it, under the 60 and 30 fps budgets. The bright window is what the
 * graph below shows. Ported from Axon Fleet's minimap: it never zooms, only
 * the window moves; drag it, drag its edges, or click to jump there.
 *
 * Lines rather than bars, and the most in each pixel column rather than the
 * mean: a minute of frames is thousands wide, and a one-frame hitch must
 * still show at any width.
 */
import { onMounted, onUnmounted, ref, watch } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import type { Window } from "./view.ts"

export type Line = { name: string; color: string; value: (frame: PerfFrame) => number; bold?: boolean }

const props = defineProps<{ frames: readonly PerfFrame[]; bounds: Window; window: Window; lines: readonly Line[] }>()
const emit = defineEmits<{ show: [startMs: number, endMs: number] }>()

const HEIGHT = 72
const HANDLE_PX = 5
const BUDGETS = [{ ms: 1000 / 60, label: "60 fps" }, { ms: 1000 / 30, label: "30 fps" }] as const

const canvas = ref<HTMLCanvasElement | null>(null)
let width = 0
let observer: ResizeObserver | null = null
let frame: number | null = null
let dirty = true

type Drag = { kind: "move"; grabMs: number } | { kind: "edge"; side: "start" | "end" } | null
let drag: Drag = null

const full = () => Math.max(props.bounds.endMs - props.bounds.startMs, 1)
const toPx = (ms: number) => ((ms - props.bounds.startMs) / full()) * width
const toMs = (px: number) => props.bounds.startMs + (px / width) * full()

/** Per pixel column, the most of `value` among the frames that end in it (NaN where none do). */
function columns(value: (f: PerfFrame) => number) {
    const out = new Float32Array(Math.max(1, Math.floor(width))).fill(Number.NaN)
    for (const f of props.frames) {
        const x = Math.min(out.length - 1, Math.max(0, Math.floor(toPx(f.end))))
        const v = value(f)
        const was = out[x]!
        out[x] = Number.isNaN(was) ? v : Math.max(was, v)
    }
    return out
}

function draw() {
    const element = canvas.value
    if (!element || width === 0) return
    const g = element.getContext("2d")
    if (!g) throw new Error("perf: no 2d context for the minimap")
    const dpr = window.devicePixelRatio || 1
    const w = Math.round(width * dpr)
    const h = Math.round(HEIGHT * dpr)
    if (element.width !== w || element.height !== h) {
        element.width = w
        element.height = h
    }
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, width, HEIGHT)

    const frameTimes = columns((f) => f.end - f.start)
    let top = 1000 / 30
    for (const v of frameTimes) if (!Number.isNaN(v)) top = Math.max(top, v)
    top = Math.min(top * 1.12, 100) // a hitch past 100 ms tops out
    const y = (ms: number) => HEIGHT - 2 - (Math.min(ms, top) / top) * (HEIGHT - 6)

    // the budgets
    g.font = "10px system-ui, sans-serif"
    g.textBaseline = "bottom"
    for (const budget of BUDGETS) {
        const by = Math.round(y(budget.ms)) + 0.5
        g.strokeStyle = "rgb(255 255 255 / 0.13)"
        g.setLineDash([3, 3])
        g.beginPath()
        g.moveTo(0, by)
        g.lineTo(width, by)
        g.stroke()
        g.setLineDash([])
        g.fillStyle = "rgb(255 255 255 / 0.32)"
        g.fillText(budget.label, 4, by - 1)
    }

    const trace = (values: Float32Array) => {
        g.beginPath()
        let pen = false
        values.forEach((v, x) => {
            if (Number.isNaN(v)) return
            if (pen) g.lineTo(x + 0.5, y(v))
            else g.moveTo(x + 0.5, y(v))
            pen = true
        })
        return pen
    }

    // the zones behind, the frame over them, filled to the floor
    for (const line of props.lines) {
        if (line.bold) continue
        if (!trace(columns(line.value))) continue
        g.strokeStyle = line.color
        g.globalAlpha = 0.85
        g.lineWidth = 1.25
        g.lineJoin = "round"
        g.stroke()
        g.globalAlpha = 1
    }
    const bold = props.lines.find((l) => l.bold)
    if (bold && trace(frameTimes)) {
        g.strokeStyle = bold.color
        g.lineWidth = 1.5
        g.lineJoin = "round"
        g.stroke()
        // the fill: the same path closed to the floor
        let first = -1
        let last = -1
        frameTimes.forEach((v, x) => {
            if (Number.isNaN(v)) return
            if (first < 0) first = x
            last = x
        })
        g.lineTo(last + 0.5, HEIGHT)
        g.lineTo(first + 0.5, HEIGHT)
        g.closePath()
        const fill = g.createLinearGradient(0, 0, 0, HEIGHT)
        fill.addColorStop(0, "rgb(230 230 240 / 0.16)")
        fill.addColorStop(1, "rgb(230 230 240 / 0.02)")
        g.fillStyle = fill
        g.fill()
    }
    // hitches past the top, marked
    g.fillStyle = "#ff6b6b"
    frameTimes.forEach((v, x) => {
        if (v > top) g.fillRect(x, 0, 1, 2)
    })

    // the window: what's off it dimmed, so the bright part reads as "here"
    const x0 = toPx(props.window.startMs)
    const x1 = toPx(props.window.endMs)
    g.fillStyle = "rgb(0 0 0 / 0.5)"
    g.fillRect(0, 0, Math.max(0, x0), HEIGHT)
    g.fillRect(Math.min(x1, width), 0, Math.max(0, width - x1), HEIGHT)
    const drawn = Math.max(x1 - x0, 2) // grabbable however far it's zoomed
    g.strokeStyle = "rgb(255 255 255 / 0.55)"
    g.lineWidth = 1
    g.strokeRect(Math.round(x0) + 0.5, 0.5, Math.round(drawn) - 1, HEIGHT - 1)
}

function loop() {
    frame = requestAnimationFrame(loop)
    if (!dirty) return
    dirty = false
    draw()
}
const invalidate = () => (dirty = true)
watch(() => [props.frames, props.lines, props.bounds.startMs, props.bounds.endMs, props.window.startMs, props.window.endMs], invalidate)

function hitEdge(x: number): "start" | "end" | null {
    if (Math.abs(x - toPx(props.window.startMs)) <= HANDLE_PX) return "start"
    if (Math.abs(x - toPx(props.window.endMs)) <= HANDLE_PX) return "end"
    return null
}
const localX = (event: MouseEvent) => event.clientX - (canvas.value?.getBoundingClientRect().left ?? 0)

function onMouseDown(event: MouseEvent) {
    const x = localX(event)
    const edge = hitEdge(x)
    if (edge) {
        drag = { kind: "edge", side: edge }
        return
    }
    const ms = toMs(x)
    if (ms >= props.window.startMs && ms <= props.window.endMs) {
        drag = { kind: "move", grabMs: ms - props.window.startMs }
        return
    }
    // outside: jump there, keeping its width, and go on as a move
    const span = props.window.endMs - props.window.startMs
    drag = { kind: "move", grabMs: span / 2 }
    emit("show", ms - span / 2, ms + span / 2)
}

function onMouseMove(event: MouseEvent) {
    const element = canvas.value
    if (!element) return
    const x = localX(event)
    if (!drag) {
        element.style.cursor = hitEdge(x) ? "ew-resize" : "pointer"
        return
    }
    const ms = toMs(x)
    if (drag.kind === "move") {
        const span = props.window.endMs - props.window.startMs
        emit("show", ms - drag.grabMs, ms - drag.grabMs + span)
        return
    }
    const least = full() / 5000 // never inverted or collapsed by a fast drag
    if (drag.side === "start") emit("show", Math.min(ms, props.window.endMs - least), props.window.endMs)
    else emit("show", props.window.startMs, Math.max(ms, props.window.startMs + least))
}

const onMouseUp = () => (drag = null)

onMounted(() => {
    const parent = canvas.value?.parentElement
    if (!parent) throw new Error("perf: the minimap isn't mounted")
    observer = new ResizeObserver((entries) => {
        width = entries[0]?.contentRect.width ?? 0
        invalidate()
    })
    observer.observe(parent)
    width = parent.getBoundingClientRect().width
    window.addEventListener("mouseup", onMouseUp)
    window.addEventListener("mousemove", onMouseMove)
    frame = requestAnimationFrame(loop)
})
onUnmounted(() => {
    observer?.disconnect()
    window.removeEventListener("mouseup", onMouseUp)
    window.removeEventListener("mousemove", onMouseMove)
    if (frame !== null) cancelAnimationFrame(frame)
})
</script>

<template>
    <div class="minimap" :style="{ height: `${HEIGHT}px` }">
        <canvas ref="canvas" @mousedown.left="onMouseDown" />
    </div>
</template>

<style scoped>
.minimap { position: relative; flex: none; width: 100%; }
canvas { display: block; width: 100%; height: 100%; }
</style>
