<script setup lang="ts">
import { computed, onBeforeUnmount, ref, toRef, watch } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { IconTile, Progress, Slider, Stages } from "../../kit/index.ts"
import { body as bodyOf, STAR, type Body } from "../../system/model.ts"
import { useSystemPreview } from "../../system/preview.ts"
import { paint, type OverlayId } from "../../worldgen/fields.ts"
import LayerRail from "../../worldgen/LayerRail.vue"
import { paintWeather } from "../../worldgen/weather.ts"
import { season } from "../../worldgen/sun.ts"
import type { WorldDraftHandle } from "./draft.ts"
import { useLooks } from "./looks.ts"
import { useBridge } from "../../bridge/use.ts"
import { encodeWorldCode } from "../../worldgen/config.ts"
import { usePlanetarium } from "../../planetarium/use.ts"
import HoverCard from "../../planetarium/HoverCard.vue"
import { useStore } from "../../game/store.ts"

// frame: what the page covers of the window's sides (Dock's covers): the planet is framed in the rest
const props = defineProps<{ draft: WorldDraftHandle; frame: { left: number; right: number } }>()
const cards = useSystemPreview()
const store = useStore()
// the bar names the stage under way, else the world as a whole
const makingLabel = computed(() => {
    const loading = store.making
    if (!loading) return ""
    const running = loading.stages.find(stage => stage.state === "running")
    return `${running?.label ?? "Making the world"} · ${Math.round(loading.share * 100)}%`
})
const looks = useLooks()
const previewer = looks.terrain
const date = looks.date
const selected = computed<string>({ get: () => props.draft.system.selected, set: (id) => { props.draft.system.selected = id } })
const chosen = computed<Body | null>(() => selected.value === STAR ? null : bodyOf(props.draft.system, selected.value))
const playable = computed(() => chosen.value?.playable ?? false)
const chosenCard = computed(() => chosen.value ? cards.card(chosen.value.id) : null)
const overlay = ref<OverlayId | "inside">("terrain")
const surfaceOverlay = computed<OverlayId>(() => overlay.value === "inside" ? "terrain" : overlay.value)
const VIEW_ICONS = {
    seams: { rows: ["..AAAAA..", ".A..B..A.", "A...B...A", "A...B...A", "ABBBBBBBA", "A...B...A", "A...B...A", ".A..B..A.", "..AAAAA.."], palette: { A: "#d5e0e8", B: "#f2c84b" } },
    inside: { rows: ["..AAAAA..", ".ABBBBBA.", "ABBCBBBBA", "ABCCDBBBA", "ABCDDBBBA", "ABCCDBBBA", ".ABBBBBA.", "..AAAAA..", "........."], palette: { A: "#d5e0e8", B: "#8b6955", C: "#c15b32", D: "#f2a03c" } },
} as const

// what a body has decides what can be shown on it: no rain without water, nothing on a giant's clouds
const DRY: readonly OverlayId[] = ["terrain", "elevation", "temperature", "plates", "geology"]
const WET: readonly OverlayId[] = [...DRY, "biomes", "moisture", "seasons", "fertility", "currents"]
const offered = computed<readonly OverlayId[] | undefined>(() => {
    const card = chosenCard.value
    if (selected.value === STAR) return undefined
    if (!card || card.giant) return []
    const shown = (card.ocean ? WET : DRY).filter((id) => id !== "plates" || card.plates) // (a lid is one plate)
    // (the moving air is a start body's, with seas: src/fluid)
    return playable.value && card.ocean && card.spin !== "star" ? [...shown, "weather"] : shown
})
watch(offered, (ids) => { if (ids && overlay.value !== "inside" && !ids.includes(overlay.value)) overlay.value = "terrain" })
watch(overlay, (value) => { if (value === "weather") looks.wantWeather() })
const paintings = new WeakMap<object, Map<OverlayId, ImageData>>() // by the world's fields
const image = computed(() => {
    const fields = previewer.fields.value
    if (!fields) return null
    if (surfaceOverlay.value !== "weather") {
        // painted once a world: switching back to an overlay is free
        let painted = paintings.get(fields)
        if (!painted) paintings.set(fields, painted = new Map())
        let picture = painted.get(surfaceOverlay.value)
        if (!picture) painted.set(surfaceOverlay.value, picture = paint(fields, surfaceOverlay.value))
        return picture
    }
    const weather = looks.weather.at(date.value)
    return weather ? paintWeather(fields, weather) : null
})
const day = computed(() => Math.floor(shownDate.value * props.draft.config.year) + 1)
// The globe is the native Planetarium under the page (src/shell/planetarium.h):
// the world's whole system, the camera round the body selected, its layer
// over the land. Shown for as long as the viewer is.
// the system's shape can change with every tick of a dragged slider: let it settle
const shown = ref({ code: encodeWorldCode(props.draft.config) })
let settling: ReturnType<typeof setTimeout> | undefined
watch(() => encodeWorldCode(props.draft.config), (code) => {
    clearTimeout(settling)
    settling = setTimeout(() => { shown.value = { code: code } }, 200)
})
onBeforeUnmount(() => clearTimeout(settling))
const planet = usePlanetarium({
    bridge: useBridge(),
    place: "whole", // the universe behind the whole screen, as behind the menu
    active: ref(true),
    frame: toRef(props, "frame"),
    body: shown,
})
const playableId = computed(() => props.draft.system.bodies.find((b) => b.playable)!.id)
// the camera flies to the body selected: in from the menu's view when the
// screen opens, across the system when another is picked; out to the whole
// system for the star
watch(selected, (id, was) => {
    const seconds = was === undefined ? 2.5 : 3
    planet.fly({ body: id, radii: 2.5, seconds: seconds })
}, { immediate: true })
// what's over the land: the overlay's painting (the playable world's), the
// body cut open, or the land itself
watch([overlay, image, selected, () => overlay.value === "temperature" ? date.value : null], () => {
    const shown = overlay.value
    if (shown === "inside") planet.layer(selected.value !== STAR ? { inside: true } : null)
    else if (shown === "terrain") planet.layer(null)
    // the weather is a year the page simulated: its painting; the rest painted natively, every body's
    else if (shown === "weather") planet.layer(playable.value && image.value ? image.value : null)
    // (only the temperature changes with the date)
    else planet.layer(shown === "temperature" ? { overlay: shown, date: date.value } : { overlay: shown })
}, { immediate: true })
// a click on the playable world is where to start: its column, as the
// engine rounds a place to one (src/mapgen/terra_column.cpp: equirect)
watch(() => store.picked, (pick) => {
    if (!pick || !playable.value) return
    const size = props.draft.config.size, half = size / 2
    let x = Math.round(pick.longitude / (2 * Math.PI) * size)
    x -= Math.floor((x + half) / size) * size
    const z = Math.min(half, Math.max(1 - half, Math.round(pick.latitude / Math.PI * size)))
    props.draft.spawn = { x: x, z: z }
})
// the clock: running, the date follows the native clock (the planets move,
// the Earth turns); scrubbing stops it there, and the climate follows the date
const running = ref(false) // stopped until asked: running, the camera goes round with the ground
// never left running behind: the menu holds still
onBeforeUnmount(() => planet.time({ running: false }))
const shownDate = computed<number>({
    get: () => running.value && store.clock ? store.clock.date : date.value,
    set: (value) => {
        running.value = false
        date.value = value
        planet.time({ running: false, date: value })
    },
})
function toggleClock() {
    running.value = !running.value
    if (!running.value && store.clock) date.value = store.clock.date // stopped where it was
    planet.time({ running: running.value, date: running.value ? undefined : date.value })
}
const seams = ref(false)
watch(seams, (shown) => planet.seams(shown))
// a double click on a body flew there: it's the one selected now
watch(() => store.chosen, (id) => {
    if (!id) return
    store.chosen = null
    selected.value = id
})
const kindNote = computed(() => chosenCard.value ? `${chosenCard.value.kind} · made by the world model` : "waiting for the body model…")
</script>

<template>
    <div class="viewer">
        <HoverCard />
        <LayerRail v-if="chosen" v-model="overlay" class="overlays" :ids="offered">
            <IconTile label="Region seams" side="left" :size="36" :on="seams" @click="seams = !seams">
                <PixelIcon :rows="VIEW_ICONS.seams.rows" :palette="VIEW_ICONS.seams.palette" style="--u: 2px" />
            </IconTile>
            <IconTile v-if="chosenCard" label="Inside cutaway" side="left" :size="36"
                :on="overlay === 'inside'" @click="overlay = 'inside'">
                <PixelIcon :rows="VIEW_ICONS.inside.rows" :palette="VIEW_ICONS.inside.palette" style="--u: 2px" />
            </IconTile>
        </LayerRail>
        <div v-if="store.making !== null" class="making" role="status">
            <div class="making-head">
                <span class="spinner" aria-hidden="true" />
                <Progress :value="store.making.share" :label="makingLabel" class="making-bar" />
            </div>
            <Stages :stages="store.making.stages" />
        </div>
        <div class="notes">
            <div v-if="chosen && !playable" class="concept-note">{{ kindNote }}</div>
            <div v-if="planet.problem.value" class="concept-note">{{ planet.problem.value }}</div>
            <div v-if="overlay === 'weather' && !image" class="concept-note">{{ looks.weather.problem.value ?? 'the weather is settling…' }}</div>
        </div>

        <div class="foot">
            <div class="date">
                <button class="clock" :aria-label="running ? 'Stop the clock' : 'Run the clock'" :title="running ? 'Stop' : 'Run'"
                    @click="toggleClock">{{ running ? "❚❚" : "▶" }}</button>
                <Slider v-model="shownDate" label="Date" :min="0" :max="0.999" :step="0.005"
                    :format="() => `${season(shownDate)} · day ${day} of ${draft.config.year}`" />
            </div>
            <p v-if="playable" class="spawn">
                <span>Spawn</span><strong>{{ draft.spawn ? `${draft.spawn.x}, ${draft.spawn.z}` : 'random land · click the planet' }}</strong>
                <button v-if="draft.spawn" @click="draft.spawn = null">reset</button>
            </p>
        </div>
    </div>
</template>

<style scoped>
/* laid against the edges of the free space: the planet is behind it all */
.viewer { position: relative; height: 100%; min-height: 0; }
.overlays { position: absolute; top: 0; right: 0; max-height: calc(100% - 140px); overflow-y: auto; }
.pending { color: var(--ink-faint); font: 12px var(--font-display); }
/* over the date, as wide: clear of the system's bodies along the top. The old world stays drawn till the new one's ready */
.making { position: absolute; left: 0; bottom: 96px; display: grid; gap: 8px; width: min(420px, 45%); padding: 8px 10px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); }
.making-head { display: flex; gap: 10px; align-items: center; min-width: 0; }
.making-bar { flex: 1; min-width: 0; }
.spinner { flex: none; width: 12px; height: 12px; background: var(--accent); animation: turn 0.8s steps(4) infinite; }
@keyframes turn { to { transform: rotate(360deg); } }
.notes { position: absolute; right: 0; bottom: 64px; display: grid; gap: 6px; justify-items: end; }
.concept-note { max-width: 240px; padding: 6px 8px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); color: var(--ink-dim); font-size: 10px; }
.foot { position: absolute; left: 0; bottom: 0; display: grid; gap: 8px; width: min(420px, 45%); }
.date { display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: end; gap: 10px; }
.clock { width: 34px; height: 34px; border: var(--b) solid var(--edge); background: var(--stone-2); color: var(--ink); font: 12px var(--font-display); cursor: pointer; }
.clock:hover { background: var(--stone-3); }
.spawn { display: flex; gap: 12px; align-items: center; margin: 0; font-size: 12px; color: var(--ink-dim); white-space: nowrap; }
.spawn strong { font: 400 12px var(--font-display); color: var(--ink); }
.spawn button { border: 0; background: none; padding: 0; color: var(--accent-hi); font-size: 12px; }
</style>
