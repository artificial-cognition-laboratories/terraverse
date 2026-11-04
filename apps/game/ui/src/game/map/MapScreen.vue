<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { useStore } from "../store.ts"
import type { PlanetariumLayer } from "../../bridge/types.ts"
import { Button, IconTile, Notice } from "../../kit/index.ts"
import PixelIcon from "../PixelIcon.vue"
import MapView from "../../worldgen/MapView.vue"
import LayerRail from "../../worldgen/LayerRail.vue"
import { OVERLAYS, type OverlayId } from "../../worldgen/fields.ts"
import { usePlanetarium } from "../../planetarium/use.ts"
import HoverCard from "../../planetarium/HoverCard.vue"
import BodyStrip from "../../system/BodyStrip.vue"
import { STAR } from "../../system/model.ts"
import { MapBodies } from "./bodies.ts"
import { mapImage, mapKey, mapUv, worldMapData } from "./data.ts"

// What the map was showing, kept for the next time it's opened: while the player's on the same body.
const memory = {
    world: "",
    opened: false,
    shown: undefined as string | undefined,
    looked: null as string | null,
    view: "globe" as "globe" | "flat",
    overlay: "terrain" as OverlayId,
}
/**
 * The map: the whole window. The globe is the native Planetarium under the
 * page (src/shell/planetarium.h), the world's system round the body shown:
 * the one the player's on, or another of its bodies chosen at the top. Out
 * to the whole system and back; the player marked where they are. The flat
 * map is the page's, of the body the player's on (what they've explored).
 */
const bridge = useBridge()
const store = useStore()
const data = worldMapData()
const map = computed(() => store.ui.map)
// the body shown: the player's, until another is chosen (and theirs again when they travel)
const here_ = map.value ? mapKey(map.value) : ""
if (memory.world !== here_) Object.assign(memory, { world: here_, opened: false, shown: map.value?.body, looked: null, view: "globe", overlay: "terrain" })
const shown = ref<string | undefined>(memory.shown)
watch(() => map.value?.body, (body) => { shown.value = body })
const onShown = computed(() => shown.value === map.value?.body)
// a body only looked at (the star, a giant: none to stand on), the camera round it; none: the shown one's
const looked = ref<string | null>(memory.looked)
const standable = (id: string) => !!map.value?.bodies?.some((b) => b.id === id)
const selected = computed(() => looked.value ?? shown.value ?? map.value?.body ?? "")
const position = computed(() => store.ui.map_position?.body === map.value?.body ? store.ui.map_position : null)
const pose = computed(() => store.state.mapPose ?? (position.value && map.value ? {
    ...mapUv(position.value.x, position.value.z, map.value.size), heading: 0, height: map.value.sea,
} : null))
const view = ref<"globe" | "flat">(memory.view)
// the world model charted for what's on screen: the flat map's (the player's body), or the globe's shown body
const charting = computed(() => map.value ? { code: map.value.code, ...(view.value === "flat" || !shown.value ? (map.value.body ? { body: map.value.body } : {}) : { body: shown.value }) } : null)
const fields = computed(() => {
    const current = data.preview.value
    return current && charting.value && current.key === mapKey(charting.value) ? current.fields : null
})
const bodies = MapBodies({
    bridge: bridge,
    code: computed(() => map.value?.code),
    charted: computed(() => fields.value && charting.value?.body ? { id: charting.value.body, fields: fields.value } : null),
})
const shownName = computed(() => {
    if (looked.value === STAR) return "The system"
    const id = looked.value ?? shown.value
    return bodies.world.value?.system.bodies.find((b) => b.id === id)?.name ?? map.value?.name ?? "World"
})
const youTag = (id: string) => id === map.value?.body ? "you" : null
const overlay = ref<OverlayId>(memory.overlay)
watch([shown, looked, view, overlay], ([s, l, v, o]) => Object.assign(memory, { shown: s, looked: l, view: v, overlay: o }))
// what can be painted over a body: every layer but the weather (it needs a year simulated); on another
// body in survival, none (the player has only explored where they are)
const LAYERS = OVERLAYS.map((o) => o.id).filter((id) => id !== "weather")
const layers = computed<readonly OverlayId[]>(() => view.value === "globe" && !onShown.value && map.value?.mode !== "creative" ? [] : LAYERS)
watch(layers, (ids) => { if (!ids.includes(overlay.value)) overlay.value = "terrain" })
const viewIcons = {
    globe: { rows: ["...AAA...", ".AABBBA..", "AABBBBAA.", "ABBBGGGBA", "ABBBGGGBA", "AABBBBBA.", ".AABBAA..", "...AAA...", "........."], palette: { A: "#d5e0e8", B: "#2680bc", G: "#62b46b" } },
    flat: { rows: ["AAAAAAAAA", "ABBBBGGBA", "ABBBGGGBA", "ABBGGGGBA", "ABBGGGBBA", "ABBBGBBBA", "ABBBBBBBA", "ABBBBBBBA", "AAAAAAAAA"], palette: { A: "#d5e0e8", B: "#2680bc", G: "#62b46b" } },
    find: { rows: ["....A....", "....A....", "...AAA...", "AAABBBAAA", "AABBBBAAA", "AAABBBAAA", "...AAA...", "....A....", "....A...."], palette: { A: "#dceaf2", B: "#36b6e8" } },
    system: { rows: ["....A....", "..A...A..", ".A.....A.", "A...B...A", "...BCB...", "A...B...A", ".A.....A.", "..A...A..", "....A...."], palette: { A: "#d5e0e8", B: "#f2c84b", C: "#fff3b0" } },
} as const
const image = computed(() => map.value && fields.value ? mapImage(fields.value, map.value, overlay.value) : null)
// on the globe: the overlay painted natively on every body (in survival, the player's own masked to what
// they've explored; the others aren't offered)
const globeLayer = computed<Extract<PlanetariumLayer, { overlay: string }> | null>(() => {
    const shownOverlay = overlay.value
    if (view.value !== "globe" || looked.value || shownOverlay === "terrain" || shownOverlay === "weather" || !map.value) return null
    return map.value.mode === "survival" && onShown.value
        ? { overlay: shownOverlay, explored: { grid: map.value.grid, mask: map.value.explored } }
        : { overlay: shownOverlay }
})
watch(globeLayer, (layer) => planet.layer(layer))
const PLAYER = "#ffe36a"
const WHOLE_GLOBE = 1.6 // radii over the sea: the whole planet in view, with room round it
const here = computed(() => pose.value ? { latitude: (0.5 - pose.value.v) * Math.PI, longitude: (pose.value.u - 0.5) * 2 * Math.PI } : null)
const planet = usePlanetarium({
    bridge,
    place: "whole",
    active: computed(() => view.value === "globe" && !!map.value),
    body: computed(() => map.value ? (shown.value ? { code: map.value.code, body: shown.value } : { code: map.value.code }) : null),
    markers: computed(() => here.value && onShown.value && pose.value && map.value ? [{ ...here.value, colour: PLAYER, altitude: Math.max(0, pose.value.height - map.value.sea) }] : []),
    // over the player the first time; after that, where it was left
    start: () => memory.opened ? null : here.value && map.value ? { ...here.value, altitude: map.value.radius * WHOLE_GLOBE } : null,
})
memory.opened = true
store.picked = null
function findMe() {
    looked.value = null
    if (!map.value?.body || onShown.value) {
        if (here.value) planet.focus(here.value)
        return
    }
    shown.value = map.value.body
}
// another body of the world: its own land drawn round it, the camera flown there
function choose(id: string) {
    if (id === STAR) {
        // the star close up; out from it, the whole system
        looked.value = STAR
        return planet.fly({ body: STAR, radii: 2.5, seconds: 3 })
    }
    if (!standable(id)) {
        // only looked at: the camera to it, the land shown stays
        looked.value = id
        return planet.fly({ body: id, radii: 2.5, seconds: 3 })
    }
    looked.value = null
    if (id === shown.value) return planet.fly({ body: id, radii: 2.5, seconds: 3 })
    shown.value = id
    store.picked = null
}
watch(shown, (id, was) => { if (id && was) planet.fly({ body: id, radii: 2.5, seconds: 3 }) })
// the whole system, the shown body at its middle
function outToSystem() {
    looked.value = STAR
    planet.fly({ ...(shown.value ? { body: shown.value } : {}), radii: 400, seconds: 2.5 })
}
// a double click flew to a body: one the player can stand on is the one shown
// (its land drawn); another (a giant, the star) is only looked at
watch(() => store.chosen, (id) => {
    if (!id) return
    store.chosen = null
    if (!standable(id)) looked.value = id
    else {
        looked.value = null
        if (id !== shown.value) {
            shown.value = id
            store.picked = null
        }
    }
})
// creative: go to the place clicked on the shown body (tf_player checks it, and moves the player)
const problem = ref<string | null>(null)
function goThere() {
    const p = store.picked
    if (!p) return
    const payload = { latitude: p.latitude, longitude: p.longitude, ...(shown.value ? { body: shown.value } : {}) }
    void bridge.send("ui.send", { name: "map.teleport", payload: payload })
        .then(() => void bridge.send("screen.close", {}))
        .catch((error: unknown) => {
            problem.value = error instanceof Error ? error.message : String(error)
            console.error("map.teleport", error)
        })
}
const picked = computed(() => store.picked ? {
    lat: (store.picked.latitude * 180 / Math.PI).toFixed(2), lon: (store.picked.longitude * 180 / Math.PI).toFixed(2),
    cell: `${store.picked.cell.region} ${store.picked.cell.index} · ${store.picked.cell.i}, ${store.picked.cell.j}`,
} : null)
const flatMarker = ref<{ x: number; z: number } | null>(null)

watch(() => charting.value ? mapKey(charting.value) : null, () => { if (charting.value) void data.ensure(bridge, charting.value) }, { immediate: true })
watch(() => [pose.value?.u, pose.value?.v, map.value?.size], ([u, v, size]) => {
    if (u === undefined || v === undefined || size === undefined) return
    flatMarker.value = { x: u * size, z: v * size }
}, { immediate: true })
</script>

<template>
    <div class="map-screen" :class="{ planet: view === 'globe' }">
        <HoverCard v-if="view === 'globe'" />
        <header class="top">
            <strong class="title">{{ view === 'flat' ? (map?.name ?? 'World') : shownName }}</strong>
            <BodyStrip v-if="view === 'globe' && bodies.world.value" class="strip" :system="bodies.world.value.system" :star="bodies.world.value.star"
                :selected="selected" :image="bodies.image" :tag="youTag" @pick="choose" />
            <Button size="sm" @click="bridge.send('screen.close', {})">Done</Button>
        </header>
        <template v-if="view === 'flat'">
            <div v-if="map && image" class="flat-stage"><MapView v-model:spawn="flatMarker" :image="image" :size="map.size" :allow-spawn="false" /></div>
            <div v-else class="pending">{{ data.problem.value ?? 'Charting planet…' }}</div>
        </template>
        <nav class="view-tools" aria-label="Map views">
            <IconTile label="Globe view" :size="36" :on="view === 'globe'" @click="view = 'globe'">
                <PixelIcon :rows="viewIcons.globe.rows" :palette="viewIcons.globe.palette" style="--u: 2px" />
            </IconTile>
            <IconTile label="Flat map" :size="36" :on="view === 'flat'" @click="view = 'flat'">
                <PixelIcon :rows="viewIcons.flat.rows" :palette="viewIcons.flat.palette" style="--u: 2px" />
            </IconTile>
            <IconTile v-if="view === 'globe'" label="Find me" :size="36" @click="findMe">
                <PixelIcon :rows="viewIcons.find.rows" :palette="viewIcons.find.palette" style="--u: 2px" />
            </IconTile>
            <IconTile v-if="view === 'globe'" label="The whole system" :size="36" @click="outToSystem">
                <PixelIcon :rows="viewIcons.system.rows" :palette="viewIcons.system.palette" style="--u: 2px" />
            </IconTile>
        </nav>
        <LayerRail v-if="layers.length" v-model="overlay" class="overlays" :ids="layers" />
        <div v-if="picked && view === 'globe'" class="picked">
            <span>{{ picked.lat }}°, {{ picked.lon }}° · {{ picked.cell }}</span>
            <Button v-if="map?.mode === 'creative'" size="sm" @click="goThere">Go there</Button>
        </div>
        <Notice v-if="problem ?? planet.problem.value ?? data.problem.value ?? bodies.problem.value" class="problem" tone="danger">{{ problem ?? planet.problem.value ?? data.problem.value ?? bodies.problem.value }}</Notice>
    </div>
</template>

<style scoped>
.map-screen { position: fixed; inset: 0; z-index: 5; background: #07111a; }
/* the globe is the native planet under the page: let it show through */
.map-screen.planet { background: transparent; }
.top { position: absolute; z-index: 3; top: 0; left: 0; right: 0; pointer-events: none; display: flex; align-items: flex-start; gap: 16px; padding: 12px 14px; background: linear-gradient(rgb(4 9 13 / .82), transparent); }
.title { color: var(--accent-hi); font: 14px var(--font-display); text-transform: uppercase; letter-spacing: .06em; white-space: nowrap; margin-top: 7px; }
.top > :last-child { margin-left: auto; }
/* the bar is as tall as the strip; between its pieces the tools under it take the clicks */
.top > * { pointer-events: auto; }
/* the bodies to go to, top centre between the title and Done */
.strip { flex: 0 1 auto; min-width: 0; margin-left: auto; }
.flat-stage { position: absolute; inset: 56px 14px 60px; display: grid; place-items: center; }
.flat-stage :deep(.map) { width: min(100%, calc(100vh - 140px)); aspect-ratio: 1; border: 0; }
.view-tools, .overlays { position: absolute; z-index: 2; top: 64px; display: grid; gap: 6px; }
.view-tools { left: 14px; }
.overlays { right: 14px; }
.picked { position: absolute; z-index: 2; left: 14px; bottom: 14px; display: flex; gap: 12px; align-items: center; padding: 8px 10px; border: var(--b) solid var(--edge); background: rgb(12 18 24 / .9); color: var(--ink-dim); font: 10px var(--font-body); box-shadow: var(--raised); }
.pending { position: absolute; inset: 0; display: grid; place-items: center; color: var(--ink-dim); font: 13px var(--font-display); }
.problem { position: absolute; z-index: 2; left: 14px; bottom: 56px; }
</style>
