<script setup lang="ts">
/**
 * A capture's zones over the window the minimap holds: a rail per thread, all
 * of them always (Main first), zones nested downward in time, frame boundaries
 * on the ruler. Click a rail's head to fold it to just that head.
 *
 * Everything is local: the capture was fetched whole, so zoom and pan only
 * redraw. Each depth is drawn from the first zone in view onward, and zones
 * thinner than a pixel are drawn once per pixel column, so a wide view costs
 * its width, not its zones.
 *
 * Wheel zooms about the pointer, Shift-wheel or drag pans, double-click a zone
 * to fill the view with it, click one to pick it out. Gaps in Main's top row
 * are hatched (time no zone covers: a place to add one), as is any stretch a
 * thread's ring had already moved past when it was captured.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import { type Capture, firstEndingAfter, zoneAt } from "./capture.ts"
import { colorOf } from "./colors.ts"
import type { Window } from "./view.ts"

const props = defineProps<{ capture: Capture; window: Window; picked: string | null }>()
const emit = defineEmits<{ pan: [px: number, width: number]; zoom: [scale: number, anchor: number]; show: [startMs: number, endMs: number]; pick: [name: string | null] }>()

const ROW = 18
const HEAD = 20
const RULER = 22

type Rail = { lane: Capture["lanes"][number]; top: number; height: number; open: boolean }

const folded = ref(new Set<string>())
const rails = computed<Rail[]>(() => {
    let top = 0
    return props.capture.lanes.map((lane) => {
        const open = !folded.value.has(lane.thread)
        const rail = { lane: lane, top: top, height: open ? HEAD + Math.max(1, lane.rows.length) * ROW + 6 : HEAD, open: open }
        top += rail.height
        return rail
    })
})
const height = computed(() => rails.value.reduce((h, r) => h + r.height, 0))

const box = ref<HTMLElement | null>(null)
const canvas = ref<HTMLCanvasElement | null>(null)
const ruler = ref<HTMLCanvasElement | null>(null)
const width = ref(0)
type Hover = { name: string; thread: string; start: number; end: number; x: number; y: number }
const hover = ref<Hover | null>(null)

const span = () => props.window.endMs - props.window.startMs
const xOf = (ms: number) => ((ms - props.window.startMs) / span()) * width.value
const msAt = (x: number) => props.window.startMs + (x / width.value) * span()
const unit = (ms: number) => (ms >= 100 ? `${ms.toFixed(0)} ms` : ms >= 1 ? `${ms.toFixed(2)} ms` : `${(ms * 1000).toFixed(0)} µs`)

let hatch: CanvasPattern | null = null
function hatching(g: CanvasRenderingContext2D) {
    if (hatch) return hatch
    const tile = document.createElement("canvas")
    tile.width = tile.height = 6
    const t = tile.getContext("2d")
    if (!t) throw new Error("perf: no 2d context for the hatching")
    t.strokeStyle = "rgb(255 255 255 / 0.09)"
    t.beginPath()
    t.moveTo(0, 6)
    t.lineTo(6, 0)
    t.stroke()
    hatch = g.createPattern(tile, "repeat")
    if (!hatch) throw new Error("perf: no pattern for the hatching")
    return hatch
}

function context(el: HTMLCanvasElement, h: number) {
    const dpr = window.devicePixelRatio || 1
    const w = Math.round(width.value * dpr)
    const hh = Math.round(h * dpr)
    if (el.width !== w || el.height !== hh) {
        el.width = w
        el.height = hh
        el.style.height = `${h}px`
    }
    const g = el.getContext("2d")
    if (!g) throw new Error("perf: no 2d context for the flame graph")
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, width.value, h)
    g.font = "11px system-ui, sans-serif"
    g.textBaseline = "middle"
    return g
}

const framesInView = () => props.capture.frames.filter((f) => f.end >= props.window.startMs && f.start <= props.window.endMs)

function drawRuler() {
    const el = ruler.value
    if (!el || width.value === 0) return
    const g = context(el, RULER)
    const raw = (span() / width.value) * 90 // a tick about every 90 px, at a round step
    const steps = [0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000]
    const step = steps.find((s) => s >= raw) ?? 10000
    g.fillStyle = "rgb(255 255 255 / 0.4)"
    for (let t = 0; t <= span(); t += step) {
        const x = Math.round(xOf(props.window.startMs + t)) + 0.5
        g.fillRect(x, RULER - 5, 1, 5)
        g.fillText(t === 0 ? "0" : unit(t).replace(".00 ", " "), x + 3, 7)
    }
    const frames = framesInView()
    const room = frames.length > 0 && width.value / frames.length > 44
    g.fillStyle = "rgb(120 180 255 / 0.75)"
    for (const f of frames) {
        const x = Math.round(xOf(f.start)) + 0.5
        g.fillRect(x, 12, 1, RULER - 12)
        if (room) g.fillText(`f${f.id}`, x + 3, RULER - 5)
    }
}

function draw() {
    const el = canvas.value
    if (!el || width.value === 0) return
    const g = context(el, Math.max(height.value, 1))
    const frames = framesInView()
    const { startMs, endMs } = props.window
    const names = props.capture.names

    for (const rail of rails.value) {
        const lane = rail.lane
        g.fillStyle = "rgb(255 255 255 / 0.035)"
        g.fillRect(0, rail.top, width.value, HEAD)
        g.fillStyle = lane.rows.length ? "rgb(255 255 255 / 0.75)" : "rgb(255 255 255 / 0.35)"
        g.fillText(`${rail.open ? "▾" : "▸"}  ${lane.thread}${lane.rows.length ? "" : "  · idle"}`, 6, rail.top + HEAD / 2)
        if (!rail.open) continue
        const body = rail.top + HEAD
        const bodyHeight = rail.height - HEAD

        const gone = xOf(lane.since)
        if (gone > 0) {
            g.fillStyle = hatching(g)
            g.fillRect(0, body, Math.min(gone, width.value), bodyHeight)
        }
        if (lane.thread === "Main" && frames.length) {
            g.fillStyle = hatching(g)
            g.fillRect(xOf(frames[0]!.start), body, xOf(frames[frames.length - 1]!.end) - xOf(frames[0]!.start), ROW - 1)
        }
        g.fillStyle = "rgb(120 180 255 / 0.12)"
        for (const f of frames) g.fillRect(Math.round(xOf(f.start)), body, 1, bodyHeight)

        lane.rows.forEach((row, depth) => {
            const y = body + depth * ROW
            let lastColumn = -1
            for (let i = firstEndingAfter(row, startMs); i < row.starts.length && row.starts[i]! <= endMs; i++) {
                const x0 = xOf(row.starts[i]!)
                const x1 = xOf(row.ends[i]!)
                const name = names[row.names[i]!]!
                const w = x1 - x0
                // under a pixel: one mark per pixel column, the first zone's colour
                if (w < 1) {
                    const column = Math.floor(x0)
                    if (column === lastColumn) continue
                    lastColumn = column
                }
                const x = Math.max(0, x0)
                const drawn = Math.max(1, Math.min(width.value, x1) - x)
                g.globalAlpha = props.picked && props.picked !== name ? 0.3 : 1
                g.fillStyle = colorOf(name)
                g.fillRect(x, y, drawn - (drawn > 3 ? 1 : 0), ROW - 1)
                if (drawn > 36) {
                    g.save()
                    g.beginPath()
                    g.rect(x, y, drawn - 2, ROW)
                    g.clip()
                    g.fillStyle = "#111115"
                    g.fillText(`${name}  ${unit(row.ends[i]! - row.starts[i]!)}`, x + 4, y + ROW / 2)
                    g.restore()
                }
            }
        })
        g.globalAlpha = 1
    }
}

let pending: number | null = null
function redraw() {
    if (pending !== null) return
    pending = requestAnimationFrame(() => {
        pending = null
        drawRuler()
        draw()
    })
}
watch([rails, () => props.window.startMs, () => props.window.endMs, () => props.picked], redraw)

function hit(x: number, y: number) {
    const rail = rails.value.find((r) => y >= r.top && y < r.top + r.height)
    if (!rail) return null
    if (y < rail.top + HEAD) return { rail: rail, head: true, zone: null }
    const row = rail.lane.rows[Math.floor((y - rail.top - HEAD) / ROW)]
    if (!row) return { rail: rail, head: false, zone: null }
    const i = zoneAt(row, msAt(x), span() / width.value)
    const zone = i < 0 ? null : { name: props.capture.names[row.names[i]!]!, start: row.starts[i]!, end: row.ends[i]! }
    return { rail: rail, head: false, zone: zone }
}
function local(event: MouseEvent) {
    const rect = canvas.value?.getBoundingClientRect()
    return rect ? { x: event.clientX - rect.left, y: event.clientY - rect.top } : { x: 0, y: 0 }
}

let drag: { x: number; moved: boolean } | null = null
function onDown(event: MouseEvent) {
    drag = { x: event.clientX, moved: false }
}
function onMove(event: MouseEvent) {
    if (drag && event.buttons & 1) {
        const dx = event.clientX - drag.x
        if (Math.abs(dx) > 2 || drag.moved) {
            drag.moved = true
            emit("pan", -dx, width.value)
            drag.x = event.clientX
        }
        hover.value = null
        return
    }
    const at = local(event)
    const found = hit(at.x, at.y)
    hover.value = found?.zone ? { ...found.zone, thread: found.rail.lane.thread, x: at.x, y: at.y } : null
}
function onUp(event: MouseEvent) {
    if (drag && !drag.moved) {
        const at = local(event)
        const found = hit(at.x, at.y)
        if (found?.head) {
            const next = new Set(folded.value)
            const thread = found.rail.lane.thread
            if (next.has(thread)) next.delete(thread)
            else next.add(thread)
            folded.value = next
        } else {
            emit("pick", found?.zone?.name ?? null)
        }
    }
    drag = null
}
function onWheel(event: WheelEvent) {
    const at = local(event)
    if (event.shiftKey || Math.abs(event.deltaX) > Math.abs(event.deltaY)) {
        emit("pan", event.shiftKey ? event.deltaY : event.deltaX, width.value)
        return
    }
    const anchor = at.x >= width.value - 4 ? 1 : Math.min(1, Math.max(0, at.x / width.value))
    emit("zoom", Math.exp(event.deltaY * 0.0015), anchor)
}
function onDouble(event: MouseEvent) {
    const at = local(event)
    const zone = hit(at.x, at.y)?.zone
    if (!zone) return
    const pad = (zone.end - zone.start) * 0.05
    emit("show", zone.start - pad, zone.end + pad)
}

let observer: ResizeObserver | null = null
onMounted(() => {
    const el = box.value
    if (!el) throw new Error("perf: the flame graph isn't mounted")
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
    <div ref="box" class="flame">
        <canvas ref="ruler" class="ruler" />
        <div class="body">
            <canvas ref="canvas" @mousedown.left="onDown" @mousemove="onMove" @mouseup.left="onUp"
                @mouseleave="hover = null; drag = null" @wheel.prevent="onWheel" @dblclick="onDouble" />
        </div>
        <div v-if="hover" class="tip" :class="{ flip: hover.x > width / 2 }" :style="{ left: `${hover.x}px`, top: `${hover.y + RULER + 14}px` }">
            <strong><i :style="{ background: colorOf(hover.name) }" />{{ hover.name }}</strong>
            <span>{{ unit(hover.end - hover.start) }} · {{ hover.thread }}</span>
        </div>
    </div>
</template>

<style scoped>
.flame { position: relative; display: flex; flex-direction: column; min-width: 0; min-height: 0; }
.ruler { display: block; flex: none; width: 100%; height: 22px; border-bottom: 1px solid var(--dt-line); }
.body { position: relative; flex: 1; min-height: 0; overflow-y: auto; overflow-x: hidden; }
.body canvas { display: block; width: 100%; cursor: grab; }
.body canvas:active { cursor: grabbing; }
.tip {
    position: absolute; z-index: 3; display: flex; flex-direction: column; gap: 1px; margin-left: 12px; padding: 6px 8px;
    pointer-events: none; background: var(--dt-menu); border: 1px solid var(--dt-menu-line); border-radius: 4px;
    box-shadow: 0 4px 14px rgb(0 0 0 / 0.4); font-size: 11px; white-space: nowrap; color: var(--dt-dim);
}
.tip.flip { transform: translateX(calc(-100% - 24px)); }
.tip strong { display: flex; align-items: center; gap: 6px; color: var(--dt-ink); font-weight: 600; }
.tip i { width: 8px; height: 8px; border-radius: 2px; }
</style>
