<script setup lang="ts">
import { computed, watch } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { useStore } from "../store.ts"
import { widgetConfig } from "./config.ts"
import WidgetFrame from "./WidgetFrame.vue"
import { mapCoordinates, mapImage, mapKey, worldMapData } from "../map/data.ts"
import LocalMap from "../map/LocalMap.vue"
import { mapUv } from "../map/data.ts"

const bridge = useBridge()
const store = useStore()
const data = worldMapData()
const map = computed(() => store.ui.map)
// where the player is on the body the map is of (none while they're between)
const position = computed(() => store.ui.map_position?.body === map.value?.body ? store.ui.map_position : null)
const fields = computed(() => {
    const current = data.preview.value
    return current && map.value && current.key === mapKey(map.value) ? current.fields : null
})
const image = computed(() => map.value && fields.value ? mapImage(fields.value, map.value) : null)
const pose = computed(() => store.state.mapPose ?? (position.value && map.value ? {
    ...mapUv(position.value.x, position.value.z, map.value.size), heading: 0,
} : null))
const coordinates = computed(() => pose.value && map.value ? mapCoordinates(pose.value, map.value.size) : null)
const width = computed(() => 128 + widgetConfig.mapScale * 24)

watch(() => [map.value ? mapKey(map.value) : null, widgetConfig.ready, widgetConfig.map] as const, ([key, ready, enabled]) => {
    if (key && ready && enabled && map.value) void data.ensure(bridge, map.value)
}, { immediate: true })
</script>

<template>
    <WidgetFrame v-if="widgetConfig.ready && widgetConfig.map && map && store.state.hud?.visible && (!store.screen || store.screen === 'pause')"
        title="Map" :corner="widgetConfig.mapCorner" interactive bare>
        <button class="open-map" :class="{ round: widgetConfig.mapShape === 1 }" :style="{ width: `${width}px` }" aria-label="Open world map"
            @click="bridge.send('screen.open', { name: 'map' })">
            <LocalMap v-if="image && pose" :image="image" :pose="pose" :size="map.size" :round="widgetConfig.mapShape === 1"
                :follow-heading="widgetConfig.mapRotate" :zoom="widgetConfig.mapZoom" />
            <div v-else class="loading">{{ data.problem.value ?? 'Charting planet…' }}</div>
            <span class="readout"><span v-if="coordinates">{{ coordinates.x }}, {{ coordinates.z }}</span><span>V · OPEN</span></span>
        </button>
    </WidgetFrame>
</template>

<style scoped>
.open-map { display: block; position: relative; padding: 4px; border: var(--b) solid var(--edge); box-shadow: var(--raised), 4px 4px 0 rgb(0 0 0 / .35); background: #111820; color: var(--ink); text-align: left; }
.open-map > :first-child { aspect-ratio: 1; }
.open-map:hover { outline: var(--b) solid var(--accent-hi); }
.open-map.round { border-radius: 50%; overflow: hidden; }
.open-map.round .readout { display: none; }
.readout { position: absolute; left: 4px; right: 4px; bottom: 4px; display: flex; justify-content: space-between; gap: 8px; padding: 6px 7px 4px; font: 9px var(--font-display); white-space: nowrap; color: white; background: linear-gradient(transparent, rgb(4 9 13 / .84)); text-shadow: 1px 1px #050b10; }
.loading { display: grid; place-items: center; font: 10px var(--font-display); color: var(--ink-dim); }
</style>
