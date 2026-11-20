<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import type { MoveScene, MoveState, SceneKind, SceneRect } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import PixelIcon from "../game/PixelIcon.vue"
import { Segmented } from "../kit/index.ts"

/**
 * Things that move, played natively under the page by the game's own code
 * from the controls a player would use, on a loop. Entries not built yet say
 * so, and play nothing.
 */
const props = defineProps<{ scenes: SceneKind; chosen: MoveScene | null }>()
const SPEEDS = [0.1, 0.25, 0.5, 1] as const
const speeds = SPEEDS.map((s) => ({ value: s, label: `×${s}` }))
const bridge = useBridge()
const problem = ref<string | null>(null)
const state = shallowRef<MoveState | null>(null)
const running = ref(true)
const speed = ref<number>(1)
const host = ref<HTMLElement | null>(null)
const card = ref<HTMLElement | null>(null)
const factsOpen = ref(true)

const fail = (err: unknown) => { problem.value = err instanceof Error ? err.message : String(err) }
const rect = (): SceneRect | null => {
    const r = host.value?.getBoundingClientRect()
    return r && r.width > 0 ? { x: r.left, y: r.top, width: r.width, height: r.height } : null
}
const right = () => card.value ? card.value.getBoundingClientRect().width + 24 : 0
async function show() {
    const where = rect(), move = props.chosen
    if (!where || !move) return
    state.value = null
    if (move.later) { await bridge.send("scene.hide", {}); return }
    await bridge.send("moves.show", { scenes: props.scenes, id: move.id, rect: where, right: right() })
    await bridge.send("moves.set", { speed: speed.value, running: running.value })
}
watch(() => props.chosen, () => { running.value = true; show().catch(fail) })
watch([speed, running], () => { if (!props.chosen?.later) bridge.send("moves.set", { speed: speed.value, running: running.value }).catch(fail) })

const sizes = new ResizeObserver(() => {
    const where = rect()
    if (where) bridge.send("scene.place", { rect: where, right: right() }).catch(fail)
})
let timer = 0
onMounted(() => {
    if (host.value) sizes.observe(host.value)
    if (card.value) sizes.observe(card.value)
    show().catch(fail)
    timer = window.setInterval(() => {
        if (props.chosen && !props.chosen.later) bridge.send("moves.state", {}).then((s) => { state.value = s }).catch(fail)
    }, 150)
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
    <div ref="host" class="scene" :class="{ empty: chosen?.later }">
        <aside v-if="chosen" ref="card" class="facts" :class="{ closed: !factsOpen }">
            <button class="fold" :aria-label="factsOpen ? 'Hide the facts' : 'Show the facts'" @click="factsOpen = !factsOpen">{{ factsOpen ? "»" : "«" }}</button>
            <div v-if="factsOpen" class="facts-body">
                <h4>{{ chosen.title }}</h4>
                <p v-if="chosen.keys" class="keys">{{ chosen.keys }}</p>
                <p class="copy">{{ chosen.note }}</p>
            </div>
        </aside>
        <p v-if="state" class="doing">{{ state.doing }} · {{ state.pace.toFixed(1) }} m/s<template v-if="state.mass"> · {{ Math.round(state.mass) }} kg</template></p>
        <div v-if="!chosen?.later" class="player">
            <button class="key" title="Restart" aria-label="Restart" @click="bridge.send('moves.restart', {}).catch(fail)"><PixelIcon class="glyph" v-bind="ICONS.restart" /></button>
            <button class="key main" :title="running ? 'Pause' : 'Play'" :aria-label="running ? 'Pause' : 'Play'" @click="running = !running"><PixelIcon class="glyph" v-bind="running ? ICONS.pause : ICONS.play" /></button>
            <span class="time">{{ state ? `${state.time.toFixed(1)} s` : "making…" }}</span>
            <span class="gap" />
            <Segmented v-model="speed" class="speeds" :options="speeds" />
        </div>
        <p v-if="problem" class="problem">{{ problem }}</p>
    </div>
</template>

<style scoped>
/* nothing painted but the floats: the page's empty pixels are the scene's, and its mouse */
.scene { position: relative; width: 100%; height: 100%; min-height: 0; }
.scene.empty { background: var(--stone-0); }
.facts { position: absolute; top: 12px; right: 12px; width: 280px; max-height: calc(100% - 72px); display: flex; flex-direction: column; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.92); box-shadow: var(--raised); }
.facts.closed { width: auto; }
.fold { align-self: flex-end; padding: 2px 8px; border: 0; background: none; color: var(--accent-hi); font: 13px var(--font-display); }
.facts-body { overflow-y: auto; display: flex; flex-direction: column; gap: 12px; padding: 0 14px 14px; }
.facts-body h4 { margin: 0; font: 400 16px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.keys { margin: 0; color: var(--accent-hi); font: 11px var(--font-display); }
.copy { margin: 0; }
.doing { position: absolute; left: 16px; right: 16px; bottom: 54px; margin: 0; color: var(--ink-dim); font-size: 11px; }
.player { position: absolute; left: 16px; right: 16px; bottom: 14px; display: flex; align-items: center; gap: 8px; }
.gap { flex: 1; }
.speeds { flex: none; width: 200px; }
.speeds :deep(button) { padding: 0 2px; font-size: 10px; }
.key { display: grid; place-items: center; flex: none; width: 32px; height: 32px; padding: 0; border: var(--b) solid var(--edge); background: var(--stone-3); box-shadow: var(--raised); cursor: pointer; }
.key:hover { background: var(--stone-4); }
.key.main { background: var(--accent-lo); }
.glyph { width: 18px; height: 18px; }
.time { min-width: 62px; padding: 0 6px; font: 13px var(--font-display); color: var(--ink); }
.problem { position: absolute; left: 12px; top: 12px; margin: 0; padding: 6px 10px; color: var(--danger); font-size: 12px; background: rgb(21 24 27 / 0.92); }
</style>
