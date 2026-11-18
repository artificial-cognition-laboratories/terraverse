<script setup lang="ts">
/**
 * Every thread's zones over the window the minimap holds: a section per
 * thread (Main first), its zones nested downward in time, frame boundaries on
 * the ruler. Collapse a section to a strip of its outermost zones.
 *
 * Wheel zooms about the pointer, Shift-wheel or drag pans, double-click a zone
 * to fill the view with it, click one to pick it out everywhere. The gaps in
 * Main's top row are hatched: time no zone covers, a place to add one. So is
 * any stretch a thread's ring has already moved past.
 */
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import type { PerfFrame, PerfZones } from "../../bridge/index.ts"
import { colorOf } from "./colors.ts"
import type { Window } from "./view.ts"

const props = defineProps<{ zones: PerfZones | null; window: Window; frames: readonly PerfFrame[]; picked: string | null }>()
const emit = defineEmits<{ pan: [px: number, width: number]; zoom: [scale: number, anchor: number]; show: [startMs: number, endMs: number]; pick: [name: string | null] }>()

const ROW = 18
const HEAD = 20
const STRIP = 6 // a collapsed section's strip
const RULER = 22
const MANY = "…"

type Bar = { name: string; thread: string; depth: number; start: number; end: number }
type Section = { thread: string; since: number; top: number; height: number; depths: number; bars: Bar[]; open: boolean }

const closed = ref(new Set<string>())
const sections = computed<Section[]>(() => {
    if (!props.zones) return []
    const zones = props.zones
    const order = [...zones.tracks].sort((a, b) => (a.thread === "Main" ? -1 : b.thread === "Main" ? 1 : a.thread.localeCompare(b.thread)))
    let top = 0
    return order.map((track) => {
        let depths = 0
        const bars = track.spans.map(([nameIndex, depth, start, end]) => {
            const name = zones.names[nameIndex]
            if (name === undefined) throw new Error(`perf: zone name ${nameIndex} wasn't sent`)
            depths = Math.max(depths, depth + 1)
            return { name: name, thread: track.thread, depth: depth, start: start, end: end }
        })
        const open = !closed.value.has(track.thread)
        const section = { thread: track.thread, since: track.since, top: top, height: HEAD + (open ? depths * ROW + 6 : STRIP + 4), depths: depths, bars: bars, open: open }
        top += section.height
        return section
    })
})
const height = computed(() => sections.value.reduce((h, s) => h + s.height, 0))

const box = ref<HTMLElement | null>(null)
const canvas = ref<HTMLCanvasElement | null>(null)
const ruler = ref<HTMLCanvasElement | null>(null)
const width = ref(0)
const hover = ref<{ bar: Bar; x: number; y: number } | null>(null)

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
    el.width = Math.round(width.value * dpr)
    el.height = Math.round(h * dpr)
    el.style.height = `${h}px`
    const g = el.getContext("2d")
    if (!g) throw new Error("perf: no 2d context for the flame graph")
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, width.value, h)
    g.font = "11px system-ui, sans-serif"
    g.textBaseline = "middle"
    return g
}

const visibleFrames = () => props.frames.filter((f) => f.end >= props.window.startMs && f.start <= props.window.endMs)

function drawRuler() {
    const el = ruler.value
    if (!el || width.value === 0) return
    const g = context(el, RULER)
    // ticks at a round step about every 90 px, counted from the window's start
    const raw = (span() / width.value) * 90
    const steps = [0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000]
    const step = steps.find((s) => s >= raw) ?? 20000
    g.fillStyle = "rgb(255 255 255 / 0.4)"
    for (let t = 0; t <= span(); t += step) {
        const x = Math.round(xOf(props.window.startMs + t)) + 0.5
        g.fillRect(x, RULER - 5, 1, 5)
        g.fillText(t === 0 ? "0" : unit(t).replace(".00 ", " "), x + 3, 7)
    }
    // frames: where each begins, numbered when there's room
    const frames = visibleFrames()
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
    const frames = visibleFrames()

    for (const s of sections.value) {
        // the section's head
        g.fillStyle = "rgb(255 255 255 / 0.035)"
        g.fillRect(0, s.top, width.value, HEAD)
        g.fillStyle = "rgb(255 255 255 / 0.75)"
        g.fillText(`${s.open ? "▾" : "▸"}  ${s.thread}`, 6, s.top + HEAD / 2)
        const body = s.top + HEAD
        const bodyHeight = s.height - HEAD

        // what the ring has moved past
        const gone = xOf(s.since)
        if (gone > 0) {
            g.fillStyle = hatching(g)
            g.fillRect(0, body, Math.min(gone, width.value), bodyHeight)
        }
        // Main's top row: hatched where no zone covers the frame
        if (s.thread === "Main" && s.open && frames.length) {
            const first = frames[0]
            const last = frames[frames.length - 1]
            if (first && last) {
                g.fillStyle = hatching(g)
                g.fillRect(xOf(first.start), body, xOf(last.end) - xOf(first.start), ROW - 1)
            }
        }
        // frame boundaries
        g.fillStyle = "rgb(120 180 255 / 0.12)"
        for (const f of frames) g.fillRect(Math.round(xOf(f.start)), body, 1, bodyHeight)

        for (const bar of s.bars) {
            if (!s.open && bar.depth > 0) continue
            const x0 = xOf(bar.start)
            const x1 = xOf(bar.end)
            if (x1 < 0 || x0 > width.value) continue
            const x = Math.max(0, x0)
            const w = Math.max(1, Math.min(width.value, x1) - x)
            const y = s.open ? body + bar.depth * ROW : body + 2
            const h = s.open ? ROW - 1 : STRIP
            const many = bar.name === MANY
            g.globalAlpha = props.picked && props.picked !== bar.name ? 0.3 : 1
            g.fillStyle = many ? "#55545d" : colorOf(bar.name)
            g.fillRect(x, y, w - (w > 3 ? 1 : 0), h)
            if (s.open && w > 36 && !many) {
                g.save()
                g.beginPath()
                g.rect(x, y, w - 2, ROW)
                g.clip()
                g.fillStyle = "#111115"
                g.fillText(`${bar.name}  ${unit(bar.end - bar.start)}`, x + 4, y + ROW / 2)
                g.restore()
            }
        }
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
watch([sections, () => props.window.startMs, () => props.window.endMs, () => props.frames, () => props.picked], redraw)

function hit(x: number, y: number): { section: Section; bar: Bar | null; head: boolean } | null {
    const section = sections.value.find((s) => y >= s.top && y < s.top + s.height)
    if (!section) return null
    if (y < section.top + HEAD) return { section: section, bar: null, head: true }
    const ms = msAt(x)
    const pixel = span() / width.value
    const depth = section.open ? Math.floor((y - section.top - HEAD) / ROW) : 0
    const bar = section.bars.find((b) => b.depth === depth && ms >= b.start - pixel && ms <= b.end + pixel) ?? null
    return { section: section, bar: bar, head: false }
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
    hover.value = found?.bar ? { bar: found.bar, x: at.x, y: at.y } : null
}
function onUp(event: MouseEvent) {
    if (drag && !drag.moved) {
        const at = local(event)
        const found = hit(at.x, at.y)
        if (found?.head) {
            const next = new Set(closed.value)
            if (next.has(found.section.thread)) next.delete(found.section.thread)
            else next.add(found.section.thread)
            closed.value = next
        } else {
            const name = found?.bar?.name ?? null
            emit("pick", name === MANY ? null : name)
        }
    }
    drag = null
}
function onWheel(event: WheelEvent) {
    const at = local(event)
    // Shift, or a sideways trackpad gesture, pans; otherwise zoom about the pointer
    if (event.shiftKey || Math.abs(event.deltaX) > Math.abs(event.deltaY)) {
        emit("pan", event.shiftKey ? event.deltaY : event.deltaX, width.value)
        return
    }
    const anchor = at.x >= width.value - 4 ? 1 : Math.min(1, Math.max(0, at.x / width.value))
    emit("zoom", Math.exp(event.deltaY * 0.0015), anchor)
}
function onDouble(event: MouseEvent) {
    const at = local(event)
    const bar = hit(at.x, at.y)?.bar
    if (!bar) return
    const pad = (bar.end - bar.start) * 0.05
    emit("show", bar.start - pad, bar.end + pad)
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

defineExpose({ width })
</script>

<template>
    <div ref="box" class="flame">
        <canvas ref="ruler" class="ruler" />
        <div class="body">
            <canvas ref="canvas" @mousedown.left="onDown" @mousemove="onMove" @mouseup.left="onUp"
                @mouseleave="hover = null; drag = null" @wheel.prevent="onWheel" @dblclick="onDouble" />
            <p v-if="!zones" class="hint">Waiting for zones…</p>
        </div>
        <div v-if="hover" class="tip" :class="{ flip: hover.x > width / 2 }" :style="{ left: `${hover.x}px`, top: `${hover.y + RULER + 14}px` }">
            <template v-if="hover.bar.name === MANY">
                <strong>Many zones, each under a pixel</strong>
                <span>{{ unit(hover.bar.end - hover.bar.start) }} · {{ hover.bar.thread }} · zoom in to see them</span>
            </template>
            <template v-else>
                <strong><i :style="{ background: colorOf(hover.bar.name) }" />{{ hover.bar.name }}</strong>
                <span>{{ unit(hover.bar.end - hover.bar.start) }} · {{ hover.bar.thread }}</span>
            </template>
        </div>
    </div>
</template>

<style scoped>
.flame { position: relative; display: flex; flex-direction: column; min-width: 0; min-height: 0; }
.ruler { display: block; flex: none; width: 100%; border-bottom: 1px solid var(--dt-line); }
.body { position: relative; flex: 1; min-height: 0; overflow-y: auto; overflow-x: hidden; }
.body canvas { display: block; width: 100%; cursor: grab; }
.body canvas:active { cursor: grabbing; }
.hint { position: absolute; inset: 0; display: grid; place-items: center; margin: 0; color: var(--dt-faint); }
.tip {
    position: absolute; z-index: 3; display: flex; flex-direction: column; gap: 1px; margin-left: 12px; padding: 6px 8px;
    pointer-events: none; background: var(--dt-menu); border: 1px solid var(--dt-menu-line); border-radius: 4px;
    box-shadow: 0 4px 14px rgb(0 0 0 / 0.4); font-size: 11px; white-space: nowrap; color: var(--dt-dim);
}
.tip.flip { transform: translateX(calc(-100% - 24px)); }
.tip strong { display: flex; align-items: center; gap: 6px; color: var(--dt-ink); font-weight: 600; }
.tip i { width: 8px; height: 8px; border-radius: 2px; }
</style>
