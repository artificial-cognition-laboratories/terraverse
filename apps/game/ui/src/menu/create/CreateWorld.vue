<script setup lang="ts">
import { computed, onMounted, provide, ref, watch } from "vue"
import { BridgeError } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Button, Dock } from "../../kit/index.ts"
import { body as bodyOf, LIMITS, STAR } from "../../system/model.ts"
import { lifetime } from "../../system/stars.ts"
import { SYSTEM_PREVIEW, SystemPreview } from "../../system/preview.ts"
import { decodeWorldCode, encodeWorldCode } from "../../worldgen/config.ts"
import { worldName } from "../../worldgen/names.ts"
import type { Screen } from "../screens.ts"
import type { SystemBodyWire, SystemWire } from "../../bridge"
import BodyStrip from "../../system/BodyStrip.vue"
import { WorldDraft } from "./draft.ts"
import DynamicsTab from "./DynamicsTab.vue"
import GameTab from "./GameTab.vue"
import Inspector from "./Inspector.vue"
import { LOOKS, Looks } from "./looks.ts"
import { worldSettings } from "./rules.ts"
import LineupPage from "./LineupPage.vue"
import StarsPage from "./StarsPage.vue"
import WorldTab from "./WorldTab.vue"
import Viewer from "./Viewer.vue"

type Group = "start" | "stars" | "lineup" | "body" | "surface" | "rules"

const emit = defineEmits<{ go: [screen: Screen] }>()
const props = defineProps<{ initialCode?: string | null; home: SystemWire }>()
const bridge = useBridge()
const draft = WorldDraft({ home: props.home })
if (props.initialCode) draft.config = decodeWorldCode(props.initialCode)
const system = SystemPreview({ bridge })
provide(SYSTEM_PREVIEW, system)
// the star comes first: a heavier one can't be as old (the engine refuses it), and the code keeps all of it
watch(() => [draft.system.bodies, draft.config.star, draft.config.starMass, draft.config.metallicity, draft.config.age, draft.config.wind, draft.config.companion, draft.rolled], () => {
    const own = draft.config.companion
    // (just inside its life: the engine works that out in fixed point)
    draft.config.age = Math.min(draft.config.age, lifetime(draft.config.starMass) * 0.98, own.kind === "none" ? LIMITS.age.max : lifetime(own.mass) * 0.98, LIMITS.age.max)
    draft.system.age = draft.config.age
    draft.system.activity = draft.config.wind
    system.request(draft.system, { ...draft.config, companion: own.kind === "none" && draft.rolled ? draft.rolled : own })
}, { deep: true, immediate: true })
// The system: what the seed and style lay out, until it's edited. Edits are what
// differs from that, and they're what the code then carries.
const baseCode = computed(() => encodeWorldCode({ ...draft.config, bodies: null }))
const ordered = (bodies: readonly SystemBodyWire[]) => [...bodies.filter((b) => b.parent === null), ...bodies.filter((b) => b.parent !== null)].map((b) => ({ ...b }))
const asText = (bodies: readonly SystemBodyWire[]) => JSON.stringify(bodies)
let asked = 0
watch(baseCode, async (next) => {
    const mine = ++asked
    try {
        const made = await bridge.send("system.world", { code: next })
        if (mine !== asked) return
        const wasGenerated = !draft.config.bodies
        draft.generated = asText(made.bodies)
        draft.rolled = draft.config.companion.kind === "none" && made.companion.kind !== "none" ? made.companion : null
        if (wasGenerated) laidOut(made.bodies)
    } catch (err) { problem.value = err instanceof Error ? err.message : String(err) }
}, { immediate: true })
function laidOut(bodies: readonly SystemBodyWire[]) {
    draft.system.bodies = bodies.map((b) => ({ ...b }))
    if (draft.system.selected !== STAR && !bodies.some((b) => b.id === draft.system.selected))
        draft.system.selected = bodies.find((b) => b.playable)!.id
}
watch(() => draft.system.bodies, (bodies) => {
    const edited = asText(bodies) !== draft.generated
    const held = draft.config.bodies
    if (!edited) { if (held) draft.config.bodies = null }
    else if (!held || asText(held) !== asText(bodies)) draft.config.bodies = ordered(bodies)
}, { deep: true })
// a pasted code's bodies are the system now
watch(() => draft.config.bodies, (bodies) => {
    if (bodies && asText(ordered(draft.system.bodies)) !== asText(bodies)) laidOut(bodies)
}, { immediate: true })
function resetSystem() {
    laidOut(JSON.parse(draft.generated))
    draft.config.bodies = null
}
const looks = Looks({ bridge, draft, cards: system })
provide(LOOKS, looks)
const startTag = (id: string) => bodyOf(draft.system, id).playable ? "start" : null

const group = ref<Group>("start")
const problem = ref<string | null>(null)
const busy = ref(false)
// how much of the window the panel takes: the planet is framed in the rest
const covers = ref({ left: 0, right: 0 })
const code = computed(() => encodeWorldCode(draft.config))
const title = computed(() => RAIL.find((item) => item.id === group.value)!.label)
const RAIL = [
    { id: "start", label: "Start", rows: ["...AAA...", "..ABBBA..", ".ABBBBBA.", "ABBBBBBBA", "ABBBBBBBA", ".ABBBBBA.", "..ABBBA..", "...AAA...", "........."], palette: { A: "#c9d4dc", B: "#40b8ec" } },
    { id: "stars", label: "Stars", rows: ["....A....", "....A....", ".B..A..B.", "..BAAAB..", "AAABABAAA", "..BAAAB..", ".B..A..B.", "....A....", "....A...."], palette: { A: "#c9d4dc", B: "#d9ae69" } },
    { id: "lineup", label: "Planets", shortLabel: "Orbit", rows: ["A........", "AA..B....", "AA..B..C.", "AA..B..C.", "AA..B..C.", "AA..B....", "A........", ".........", "........."], palette: { A: "#d9ae69", B: "#46a6d5", C: "#c9d4dc" } },
    { id: "body", label: "Body", rows: ["..AAAAA..", ".ABBBBBA.", "ABBBBBAAA", "ABBBBAAAA", "ABBBBBAAA", "ABBBBBBAA", ".ABBBBBA.", "..AAAAA..", "........."], palette: { A: "#c9d4dc", B: "#46a6d5" } },
    { id: "surface", label: "Surface", shortLabel: "Land", rows: [".........", "....A....", "...AAA...", "..AABAA..", ".AABBBAA.", "AABBBBBAA", "CCCCCCCCC", "CCCCCCCCC", "........."], palette: { A: "#bec8bd", B: "#6caa73", C: "#46a6d5" } },
    { id: "rules", label: "Rules", rows: ["..AAAAA..", ".ABBBBBA.", ".AB.A.BA.", ".ABBBBBA.", ".AB.A.BA.", ".ABBBBBA.", "..AAAAA..", "...AAA...", "....A...."], palette: { A: "#c9d4dc", B: "#72b18a" } },
] as const
// choosing from the strip opens what was chosen; the planets and surface pages stay, they're about any body
function pick(id: string) {
    draft.system.selected = id
}
watch(() => draft.system.selected, (id) => {
    if (id === STAR) group.value = "stars"
    else if (group.value === "start" || group.value === "stars" || group.value === "rules") group.value = "body"
})

onMounted(async () => {
    try {
        draft.taken = (await bridge.send("worlds.list", {})).map((world) => world.name)
        if (draft.taken.some((name) => name.toLowerCase() === draft.name.toLowerCase())) draft.name = worldName(draft.taken)
    } catch (err) { problem.value = err instanceof Error ? err.message : String(err) }
})

async function create() {
    problem.value = null
    busy.value = true
    try {
        const world = await bridge.send("worlds.create", { name: draft.name, code: code.value, settings: worldSettings(draft.mode, draft.rules, draft.config) })
        await bridge.send("play", { path: world.path })
    } catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
        busy.value = false
    }
}
</script>

<template>
    <div class="create">
    <Dock v-model:group="group" v-model:covers="covers" class="create-workbench" title="Create world" :section="title" :groups="RAIL"
        rail-label="World creator sections" width="min(470px, 42vw)">
        <template #actions>
            <Button size="sm" @click="emit('go', 'worlds')">Back</Button>
        </template>
        <template #workspace>
            <div class="controls">
                <GameTab v-if="group === 'start'" :draft="draft" />
                <StarsPage v-else-if="group === 'stars'" :draft="draft" />
                <LineupPage v-else-if="group === 'lineup'" :draft="draft" @reset="resetSystem" />
                <Inspector v-else-if="group === 'body'" :draft="draft" />
                <WorldTab v-else-if="group === 'surface'" :draft="draft" />
                <DynamicsTab v-else :draft="draft" />
            </div>
        </template>
    </Dock>
    <!-- over the planet, in what the panel leaves free -->
    <div class="visual" :style="{ left: `${covers.left + 16}px` }">
        <Viewer :draft="draft" :frame="covers" />
        <div class="top"><BodyStrip class="picker" :system="draft.system" :star="draft.config.star" :selected="draft.system.selected"
            :image="looks.image" :tag="startTag" @pick="pick" /></div>
        <div class="go">
            <span v-if="problem" class="problem" role="alert">{{ problem }}</span>
            <Button variant="primary" :disabled="busy || !draft.name.trim()" @click="create">Create & play »</Button>
        </div>
    </div>
    </div>
</template>

<style scoped>
.controls { flex: 1; min-width: 0; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: 18px; padding: 20px 18px 28px; background: var(--stone-0); }
/* the planet's between these: the strip centred over it, clear of the overlays */
.visual { position: fixed; top: 16px; right: 16px; bottom: 16px; min-width: 0; pointer-events: none; }
.visual > * { pointer-events: auto; }
.visual :deep(.viewer) { position: absolute; inset: 0; pointer-events: none; }
.visual :deep(.viewer > *) { pointer-events: auto; }
.top { position: absolute; top: 0; left: 96px; right: 96px; display: flex; justify-content: center; pointer-events: none; }
.top .picker { min-width: 0; pointer-events: auto; }
.go { position: absolute; right: 0; bottom: 0; display: flex; flex-direction: column; align-items: end; gap: 6px; }
.problem { max-width: 320px; padding: 6px 8px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); color: var(--danger); font-size: 11px; }
</style>
