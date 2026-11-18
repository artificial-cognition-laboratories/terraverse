<script setup lang="ts">
/**
 * The Live tab: what the frames cost as they stream, smooth because it's drawn
 * from the frames alone (no zones fetched). Frame time on top, then a chart
 * per kind of thread (Main, Server, the emerge and mesh workers, each kind
 * summed) with its costliest zones as lines. All share one window: wheel
 * zooms, drag pans back through the minute kept, and against the newest frame
 * it rides along. Click a moment to capture it for the Flame tab.
 */
import { computed, ref } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import { threadOrder } from "./capture.ts"
import { colorOf, IDLE } from "./colors.ts"
import LineChart, { type Series } from "./LineChart.vue"
import { View, type Window } from "./view.ts"

const props = defineProps<{ frames: readonly PerfFrame[] }>()
const emit = defineEmits<{ capture: [at: number] }>()

const BUDGETS = [{ ms: 1000 / 60, label: "60 fps" }, { ms: 1000 / 30, label: "30 fps" }] as const
const LINES = 5 // zones a chart draws

const bounds = (): Window => {
    const first = props.frames[0]
    const last = props.frames[props.frames.length - 1]
    return first && last ? { startMs: first.start, endMs: last.end } : { startMs: 0, endMs: 1 }
}
const view = View({ bounds: bounds, initialSpanMs: 10_000 })
const moved = ref(0) // bumped by every gesture: the view itself isn't reactive
const win = computed<Window>(() => {
    void moved.value
    void props.frames
    return view.current
})
const live = computed(() => {
    void moved.value
    return view.following
})
const gesture = (act: () => void) => {
    act()
    moved.value++
}

// Threads of a kind are charted together: "Emerge-3" with "Emerge-0", "MeshUpdate 2" with "MeshUpdate".
const kindOf = (thread: string) => thread.replace(/[-\s]?\d+$/, "")
const groups = computed(() => {
    const totals = new Map<string, Map<string, number>>()
    const members = new Map<string, Set<string>>()
    // ranked over the last ten seconds or so: what costs now, and cheap to redo as frames stream
    for (const f of props.frames.slice(-600)) {
        for (const [thread, parts] of Object.entries(f.threads)) {
            const kind = kindOf(thread)
            let sums = totals.get(kind)
            if (!sums) totals.set(kind, (sums = new Map()))
            let threads = members.get(kind)
            if (!threads) members.set(kind, (threads = new Set()))
            threads.add(thread)
            for (const [name, ms] of Object.entries(parts)) if (!IDLE.has(name)) sums.set(name, (sums.get(name) ?? 0) + ms)
        }
    }
    return [...totals.entries()]
        .sort(([a], [b]) => threadOrder(a, b))
        .map(([kind, sums]) => {
            const threads = [...(members.get(kind) ?? [])]
            const series: Series[] = [...sums.entries()].sort((a, b) => b[1] - a[1]).slice(0, LINES).map(([name]) => ({
                name: name,
                color: colorOf(name),
                value: (f: PerfFrame) => threads.reduce((sum, t) => sum + (f.threads[t]?.[name] ?? 0), 0),
            }))
            return { kind: kind, title: threads.length > 1 ? `${kind} ×${threads.length}` : kind, series: series }
        })
        .filter((g) => g.series.length)
})
const FRAME: Series[] = [{ name: "frame", color: "#e6e6ee", value: (f) => f.end - f.start, bold: true }]

const box = ref<HTMLElement | null>(null)
const cursor = ref<number | null>(null)
const msAt = (event: MouseEvent) => {
    const rect = box.value?.getBoundingClientRect()
    if (!rect) return null
    const { startMs, endMs } = win.value
    return startMs + ((event.clientX - rect.left) / rect.width) * (endMs - startMs)
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
            gesture(() => view.pan(-dx, box.value?.clientWidth ?? 1))
            drag.x = event.clientX
        }
    }
    cursor.value = msAt(event)
}
function onUp(event: MouseEvent) {
    if (drag && !drag.moved) {
        const at = msAt(event)
        if (at !== null) emit("capture", at)
    }
    drag = null
}
function onWheel(event: WheelEvent) {
    const width = box.value?.clientWidth ?? 1
    if (event.shiftKey || Math.abs(event.deltaX) > Math.abs(event.deltaY)) {
        gesture(() => view.pan(event.shiftKey ? event.deltaY : event.deltaX, width))
        return
    }
    const x = event.clientX - (box.value?.getBoundingClientRect().left ?? 0)
    const anchor = x >= width - 4 ? 1 : Math.min(1, Math.max(0, x / width))
    gesture(() => view.zoom(Math.exp(event.deltaY * 0.0015), anchor))
}
const spanLabel = computed(() => {
    const s = (win.value.endMs - win.value.startMs) / 1000
    return s >= 1 ? `${s.toFixed(s >= 10 ? 0 : 1)} s` : `${(s * 1000).toFixed(0)} ms`
})
</script>

<template>
    <div class="live">
        <div class="bar">
            <button class="mode" :class="{ live: live }" @click="gesture(() => view.follow())"><i />{{ live ? "Following" : "Go live" }}</button>
            <span class="faint">{{ spanLabel }} · wheel zooms · drag pans · click captures that moment</span>
        </div>
        <div v-if="frames.length" ref="box" class="charts" @mousedown.left="onDown" @mousemove="onMove" @mouseup.left="onUp"
            @mouseleave="cursor = null; drag = null" @wheel.prevent="onWheel">
            <LineChart title="Frame" :frames="frames" :window="win" :series="FRAME" :height="92" :budgets="BUDGETS" :cursor="cursor" />
            <LineChart v-for="g in groups" :key="g.kind" :title="g.title" :frames="frames" :window="win" :series="g.series" :height="70"
                :cursor="cursor" />
        </div>
        <p v-else class="waiting">Waiting for frames from the engine…</p>
    </div>
</template>

<style scoped>
.live { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.bar { display: flex; align-items: center; gap: 10px; flex: none; height: 28px; padding: 0 6px; border-bottom: 1px solid var(--dt-line); font-size: 12px; }
button { display: flex; align-items: center; border: 0; background: none; color: var(--dt-ink); font: inherit; cursor: default; }
.mode { gap: 6px; height: 22px; padding: 0 8px; border-radius: 4px; }
.mode:hover { background: var(--dt-hover); }
.mode i { width: 8px; height: 8px; border-radius: 50%; background: var(--dt-faint); }
.mode.live i { background: #5fd17a; box-shadow: 0 0 6px #5fd17a; }
.faint { min-width: 0; overflow: hidden; color: var(--dt-faint); white-space: nowrap; text-overflow: ellipsis; }
.charts { flex: 1; min-height: 0; overflow-y: auto; cursor: crosshair; }
.waiting { flex: 1; display: grid; place-items: center; margin: 0; color: var(--dt-faint); }
</style>
