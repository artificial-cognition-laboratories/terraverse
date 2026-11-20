<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import type { ReliefState, SceneRect } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import PixelIcon from "../game/PixelIcon.vue"
import { ListEmpty, ListGroup, ListItem, Segmented, WorkbenchColumns } from "../kit/index.ts"
import { useWiki } from "./nav.ts"
import { WORLD_ENTRIES } from "./worlds.ts"

/** Why the land is as it is: a real river on a real world, through the rounds of erosion that made it. */
const bridge = useBridge()
const wiki = useWiki()
const speeds = [0.5, 1, 2, 4].map((s) => ({ value: s, label: `×${s}` }))
const speed = ref(1)
const running = ref(true)
const state = shallowRef<ReliefState | null>(null)
const problem = ref<string | null>(null)
const host = ref<HTMLElement | null>(null)
const card = ref<HTMLElement | null>(null)
const groups = computed(() => {
    const out = new Map<string, typeof WORLD_ENTRIES>()
    const term = wiki.search.value.trim().toLowerCase()
    for (const e of WORLD_ENTRIES) {
        if (!`${e.title} ${e.cause} ${e.recognise} ${e.gives}`.toLowerCase().includes(term)) continue
        out.set(e.group, [...(out.get(e.group) ?? []), e])
    }
    return [...out.entries()]
})
const chosen = computed(() => WORLD_ENTRIES.find((e) => e.id === wiki.state.chosen.worlds) ?? WORLD_ENTRIES[0]!)
const fail = (err: unknown) => { problem.value = err instanceof Error ? err.message : String(err) }
const rect = (): SceneRect | null => {
    const r = host.value?.getBoundingClientRect()
    return r && r.width > 0 ? { x: r.left, y: r.top, width: r.width, height: r.height } : null
}
const right = () => card.value ? card.value.getBoundingClientRect().width + 24 : 0
const set = () => bridge.send("relief.set", { lens: chosen.value.lens, speed: speed.value, running: running.value })
async function show() {
    const where = rect()
    if (!where) return
    await bridge.send("relief.show", { rect: where, right: right() })
    await set()
}
watch(chosen, () => { running.value = true; set().catch(fail) })
watch([speed, running], () => { set().catch(fail) })
const sizes = new ResizeObserver(() => {
    const where = rect()
    if (where) bridge.send("scene.place", { rect: where, right: right() }).catch(fail)
})
let timer = 0
onMounted(() => {
    if (host.value) sizes.observe(host.value)
    if (card.value) sizes.observe(card.value)
    show().catch(fail)
    timer = window.setInterval(() => { bridge.send("relief.state", {}).then((s) => { state.value = s }).catch(fail) }, 200)
})
onBeforeUnmount(() => {
    clearInterval(timer)
    sizes.disconnect()
    bridge.send("scene.hide", {}).catch(fail)
})
const ink = { I: "#d7e0e6" }
const ICONS = {
    restart: { palette: ink, rows: ["..IIIII.I", ".I.....II", "I....IIII", "I........", "I.......I", "I.......I", ".I.....I.", "..IIIII..", "........."] },
    play: { palette: ink, rows: [".I.......", ".III.....", ".IIIII...", ".IIIIIII.", ".IIIIIIII", ".IIIIIII.", ".IIIII...", ".III.....", ".I......."] },
    pause: { palette: ink, rows: [".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III.", ".III.III."] },
}
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <ListGroup v-for="[group, list] in groups" :key="group" :title="group">
                <ListItem v-for="e in list" :key="e.id" :on="chosen.id === e.id" @click="wiki.go('worlds', e.id)">{{ e.title }}</ListItem>
            </ListGroup>
            <ListEmpty v-if="!groups.length">No matches.</ListEmpty>
        </template>
        <template #content>
            <div ref="host" class="scene">
                <aside ref="card" class="facts">
                    <h4>{{ chosen.title }}</h4>
                    <h5>Why</h5><p>{{ chosen.cause }}</p>
                    <h5>How to spot it</h5><p>{{ chosen.recognise }}</p>
                    <h5>What it gives you</h5><p>{{ chosen.gives }}</p>
                </aside>
                <div class="player">
                    <button class="key" title="Restart" aria-label="Restart" @click="bridge.send('relief.restart', {}).catch(fail)"><PixelIcon class="glyph" v-bind="ICONS.restart" /></button>
                    <button class="key main" :title="running ? 'Pause' : 'Play'" :aria-label="running ? 'Pause' : 'Play'" @click="running = !running"><PixelIcon class="glyph" v-bind="running ? ICONS.pause : ICONS.play" /></button>
                    <span class="time">{{ state ? `Round ${Math.floor(state.round)} of ${state.rounds}` : "making…" }}</span>
                    <span class="gap" />
                    <Segmented v-model="speed" class="speeds" :options="speeds" />
                </div>
                <p v-if="problem" class="problem">{{ problem }}</p>
            </div>
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
/* nothing painted but the floats: the page's empty pixels are the scene's, and its mouse */
.scene { position: relative; width: 100%; height: 100%; min-height: 0; }
.facts { position: absolute; top: 12px; right: 12px; width: 280px; max-height: calc(100% - 72px); overflow-y: auto; display: flex; flex-direction: column; gap: 6px; padding: 14px; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.92); box-shadow: var(--raised); }
.facts h4 { margin: 0; font: 400 16px var(--font-display); text-transform: uppercase; }
.facts h5 { margin: 8px 0 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.facts p { margin: 0; }
.player { position: absolute; left: 16px; right: 16px; bottom: 14px; display: flex; align-items: center; gap: 8px; }
.gap { flex: 1; }
.speeds { flex: none; width: 200px; }
.speeds :deep(button) { padding: 0 2px; font-size: 10px; }
.key { display: grid; place-items: center; flex: none; width: 32px; height: 32px; padding: 0; border: var(--b) solid var(--edge); background: var(--stone-3); box-shadow: var(--raised); cursor: pointer; }
.key:hover { background: var(--stone-4); }
.key.main { background: var(--accent-lo); }
.glyph { width: 18px; height: 18px; }
.time { padding: 0 6px; font: 13px var(--font-display); color: var(--ink); }
.problem { position: absolute; left: 12px; top: 12px; margin: 0; padding: 6px 10px; color: var(--danger); font-size: 12px; background: rgb(21 24 27 / 0.92); }
</style>
