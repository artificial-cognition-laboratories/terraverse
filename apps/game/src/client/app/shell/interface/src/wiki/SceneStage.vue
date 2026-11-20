<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { SandboxSpec, SceneRect } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import PixelIcon from "../game/PixelIcon.vue"
import { Segmented } from "../kit/index.ts"
import { useTable } from "./nav.ts"
import { SandboxRun, SPEEDS } from "./sandbox.ts"

/**
 * A scene: a few blocks of the sandbox, drawn natively under this empty
 * element (src/shell/scene.h), with the page's panels floating over it. The
 * mouse turns it wherever the page has nothing painted.
 */
const props = defineProps<{ spec: SandboxSpec | null; speed?: number }>()
const bridge = useBridge()
const store = useStore()
const table = useTable()
const run = SandboxRun({ bridge: bridge })
const heat = ref(false)
const speeds = SPEEDS.map((s) => ({ value: s, label: `×${s}` }))
const host = ref<HTMLElement | null>(null)

const fail = (what: string) => (err: unknown) => console.error(what, err)
const rect = (): SceneRect | null => {
    const r = host.value?.getBoundingClientRect()
    return r && r.width > 0 ? { x: r.left, y: r.top, width: r.width, height: r.height } : null
}
// what the facts card covers: the blocks are framed in the rest
const factsCard = ref<HTMLElement | null>(null)
const right = () => factsCard.value ? factsCard.value.getBoundingClientRect().width + 24 : 0
function show() {
    const id = run.shown.value, where = rect()
    if (id === null || !where) return
    bridge.send("scene.show", { id: id, rect: where, right: right(), heat: heat.value }).catch(fail("scene.show"))
}
watch([run.shown, heat], show)
watch(() => props.spec, (spec) => {
    run.speed.value = props.speed ?? SPEEDS[0]
    run.running.value = true
    if (spec) void run.start(spec)
    else {
        void run.dispose()
        bridge.send("scene.hide", {}).catch(fail("scene.hide"))
    }
}, { immediate: true })

const sizes = new ResizeObserver(() => place())
const place = () => { const where = rect(); if (where && run.shown.value !== null) bridge.send("scene.place", { rect: where, right: right() }).catch(fail("scene.place")) }
onMounted(() => {
    if (host.value) sizes.observe(host.value)
    if (factsCard.value) sizes.observe(factsCard.value)
    show()
})
onBeforeUnmount(() => {
    sizes.disconnect()
    bridge.send("scene.hide", {}).catch(fail("scene.hide"))
    void run.dispose()
})

const clock = (s: number) => s < 120 ? `${s.toFixed(1)} s` : s < 7200 ? `${(s / 60).toFixed(1)} min` : `${(s / 3600).toFixed(1)} h`
const reading = computed(() => {
    const f = run.frame.value, i = store.sceneHover
    if (!f || i === null || i < 0 || i >= f.main.length) return null
    const parts = [`${Math.round(f.temperature[i]!)} K (${Math.round(f.temperature[i]! - 273.15)} °C)`]
    if (f.main[i]! >= 0) parts.unshift(`${table.substances[f.main[i]!]!.name} · ${f.state[i]} · ${Math.round(f.fill[i]! * 100)}% full`)
    else parts.unshift("gas")
    if (f.vapour[i]! >= 0 && f.haze[i]! > 0.02) parts.push(`${table.substances[f.vapour[i]!]!.name} ${Math.round(f.haze[i]! * 100)}% of its gas`)
    if (Math.abs(f.activity[i]!) > 0.05) parts.push(`${f.activity[i]! > 0 ? "giving off" : "taking in"} ${Math.abs(f.activity[i]!).toFixed(1)} W`)
    return parts.join(" · ")
})
const factsOpen = ref(true)
const ink = { I: "#d7e0e6" }
const ICONS = {
    restart: { palette: ink, rows: ["..IIIII.I", ".I.....II", "I....IIII", "I........", "I.......I", "I.......I", ".I.....I.", "..IIIII..", "........."] },
    play: { palette: ink, rows: [".I.......", ".III.....", ".IIIII...", ".IIIIIII.", ".IIIIIIII", ".IIIIIII.", ".IIIII...", ".III.....", ".I......."] },
    pause: { palette: ink, rows: [".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III."] },
    heat: { palette: { I: "#d7e0e6", R: "#e0603d" }, rows: ["...III...", "...I.I...", "...I.I...", "...IRI...", "...IRI...", "..IRRRI..", "..IRRRI..", "..IRRRI..", "...III..."] },
}
</script>

<template>
    <div ref="host" class="scene" :class="{ empty: run.shown.value === null }">
        <div v-if="$slots.top" class="top"><slot name="top" /></div>
        <aside v-if="$slots.facts" ref="factsCard" class="float facts" :class="{ closed: !factsOpen }">
            <button class="fold" :aria-label="factsOpen ? 'Hide the facts' : 'Show the facts'" @click="factsOpen = !factsOpen">{{ factsOpen ? "»" : "«" }}</button>
            <div v-if="factsOpen" class="facts-body"><slot name="facts" /></div>
        </aside>
        <div v-if="$slots.corner" class="corner"><slot name="corner" /></div>
        <p v-if="reading" class="reading">{{ reading }}</p>
        <div v-if="spec" class="player">
            <button class="key" title="Restart" aria-label="Restart" @click="run.restart()"><PixelIcon class="glyph" v-bind="ICONS.restart" /></button>
            <button class="key main" :title="run.running.value ? 'Pause' : 'Play'" :aria-label="run.running.value ? 'Pause' : 'Play'"
                @click="run.running.value = !run.running.value"><PixelIcon class="glyph" v-bind="run.running.value ? ICONS.pause : ICONS.play" /></button>
            <span class="time">{{ run.frame.value ? clock(run.frame.value.time) : "making…" }}</span>
            <span class="gap" />
            <Segmented v-model="run.speed.value" class="speeds" :options="speeds" />
            <button class="key" :class="{ on: heat }" title="Show heat" aria-label="Show heat" :aria-pressed="heat" @click="heat = !heat"><PixelIcon class="glyph" v-bind="ICONS.heat" /></button>
        </div>
        <p v-if="run.problem.value" class="float problem">{{ run.problem.value }}</p>
    </div>
</template>

<style scoped>
/* nothing painted but the floats: the page's empty pixels are the scene's, and its mouse */
.scene { position: relative; width: 100%; height: 100%; min-height: 0; }
.scene.empty { background: var(--stone-0); } /* no scene drawn: nothing behind the panel to show through */
.top, .reading, .player { position: absolute; }
.float { position: absolute; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.92); box-shadow: var(--raised); }
.top { top: 14px; left: 16px; max-width: calc(100% - 340px); }
.facts { top: 12px; right: 12px; max-height: calc(100% - 72px); width: 280px; display: flex; flex-direction: column; }
.facts.closed { width: auto; }
.fold { align-self: flex-end; padding: 2px 8px; border: 0; background: none; color: var(--accent-hi); font: 13px var(--font-display); }
.facts-body { flex: 1; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: 12px; padding: 0 14px 14px; }
.facts-body :deep(h4) { margin: 0; font: 400 16px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.facts-body :deep(h5) { margin: 6px 0 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.facts-body :deep(dl) { display: grid; gap: 4px; margin: 0; }
.facts-body :deep(dl div) { display: flex; justify-content: space-between; gap: 10px; font-size: 11px; }
.facts-body :deep(dt) { color: var(--ink-dim); }
.facts-body :deep(dd) { margin: 0; font-family: var(--font-display); font-size: 11px; text-align: right; }
.facts-body :deep(.links) { display: flex; flex-wrap: wrap; gap: 4px 12px; }
.corner { position: absolute; left: 16px; bottom: 72px; }
.reading { left: 16px; right: 16px; bottom: 54px; margin: 0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; color: var(--ink-dim); font-size: 11px; }
.player { left: 16px; right: 16px; bottom: 14px; display: flex; align-items: center; gap: 8px; }
.gap { flex: 1; }
.speeds { flex: none; width: 200px; }
.speeds :deep(button) { padding: 0 2px; font-size: 10px; }
.key { display: grid; place-items: center; flex: none; width: 32px; height: 32px; padding: 0; border: var(--b) solid var(--edge); background: var(--stone-3); box-shadow: var(--raised); cursor: pointer; }
.key:hover { background: var(--stone-4); }
.key.main { background: var(--accent-lo); }
.key.on { box-shadow: var(--sunk); background: var(--stone-1); }
.glyph { width: 18px; height: 18px; }
.time { min-width: 62px; padding: 0 6px; font: 13px var(--font-display); color: var(--ink); }
.problem { left: 12px; top: 60px; margin: 0; padding: 6px 10px; color: var(--danger); font-size: 12px; }
</style>
