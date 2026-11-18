<script setup lang="ts">
/**
 * The Flame tab: one capture, looked through locally. The minimap runs edge to
 * edge over its frames (frame time as a line) and holds the window the flame
 * graph shows; a drawer at the foot sums the window by zone.
 */
import { computed, ref, shallowRef, watch } from "vue"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import { aggregate, type ZoneRow } from "./aggregate.ts"
import type { Capture } from "./capture.ts"
import { colorOf } from "./colors.ts"
import FlameGraph from "./FlameGraph.vue"
import Minimap, { type Line } from "./Minimap.vue"
import { View, type Window } from "./view.ts"

const props = defineProps<{ capture: Capture }>()

const bounds = (): Window => ({ startMs: props.capture.from, endMs: props.capture.to })
const view = shallowRef(View({ bounds: bounds }))
const moved = ref(0) // bumped by every gesture: the view itself isn't reactive
watch(() => props.capture, () => {
    view.value = View({ bounds: bounds }) // a new capture opens whole
    moved.value++
})
const win = computed<Window>(() => {
    void moved.value
    return view.value.current
})
const gesture = (act: () => void) => {
    act()
    moved.value++
}

const LINES: Line[] = [{ name: "Frame", color: "#e6e6ee", value: (f) => f.end - f.start, bold: true }]
const picked = ref<string | null>(null)

const open = ref(false)
const rows = computed<ZoneRow[]>(() => (open.value ? aggregate(props.capture, win.value.startMs, win.value.endMs) : []))
const framesIn = computed(() => props.capture.frames.filter((f) => f.end > win.value.startMs && f.start < win.value.endMs).length)
const ms = (v: number) => (v >= 10 ? v.toFixed(1) : v.toFixed(2))
const columns: Column<ZoneRow>[] = [
    { id: "name", header: "Zone", flex: 2, value: (r) => r.name, swatch: (r) => colorOf(r.name) },
    { id: "self", header: "Self ms", width: 76, mono: true, value: (r) => ms(r.self) },
    { id: "total", header: "Total ms", width: 80, mono: true, value: (r) => ms(r.total) },
    { id: "calls", header: "Calls", width: 60, mono: true, muted: true, value: (r) => String(r.calls) },
    { id: "max", header: "Max ms", width: 72, mono: true, muted: true, value: (r) => ms(r.max) },
    { id: "threads", header: "Threads", flex: 1, muted: true, value: (r) => r.threads.join(", ") },
]
</script>

<template>
    <div class="flame-pane">
        <Minimap :frames="capture.frames" :bounds="bounds()" :window="win" :lines="LINES"
            @show="(a, b) => gesture(() => view.show(a, b))" @zoom="(s, a) => gesture(() => view.zoom(s, a))" />
        <FlameGraph class="graph" :capture="capture" :window="win" :picked="picked"
            @pan="(px, w) => gesture(() => view.pan(px, w))" @zoom="(s, a) => gesture(() => view.zoom(s, a))"
            @show="(a, b) => gesture(() => view.show(a, b))" @pick="(name) => (picked = name)" />
        <div class="drawer" :class="{ open: open }">
            <button class="drawer-bar" @click="open = !open">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path :d="open ? 'M4 6l4 4 4-4' : 'M4 10l4-4 4 4'" /></svg>
                Summary <span class="faint">· {{ framesIn }} frames · {{ ms(win.endMs - win.startMs) }} ms</span>
                <span v-if="picked" class="faint">· picked {{ picked }}</span>
            </button>
            <div v-if="open" class="table">
                <DataTable :columns="columns" :rows="rows" :row-key="(r) => r.name" :row-class="(r) => (picked === r.name ? 'picked' : undefined)"
                    @select="(r) => (picked = picked === r.name ? null : r.name)" />
            </div>
        </div>
    </div>
</template>

<style scoped>
.flame-pane { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.flame-pane > :first-child { border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
.graph { flex: 1; min-height: 0; }
.drawer { flex: none; display: flex; flex-direction: column; border-top: 1px solid var(--dt-line); }
.drawer.open { height: 38%; min-height: 120px; }
.drawer-bar { display: flex; align-items: center; flex: none; gap: 6px; height: 26px; padding: 0 8px; border: 0; color: var(--dt-ink); font: inherit; font-size: 12px; background: var(--dt-rail); cursor: default; }
.drawer-bar:hover { background: var(--dt-hover); }
.drawer-bar svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
.faint { color: var(--dt-faint); }
.table { flex: 1; min-height: 0; }
.table :deep(.picked) { background: var(--dt-held); }
</style>
