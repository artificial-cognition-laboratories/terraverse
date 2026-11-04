<script setup lang="ts">
import { onMounted, reactive, watch } from "vue"
import Hud from "../../game/Hud.vue"
import PauseScreen from "../../game/PauseScreen.vue"
import MapScreen from "../../game/map/MapScreen.vue"
import { useStore } from "../../game/store.ts"
import { Section, Slider, Toggle } from "../../kit/index.ts"
import { mockWorldControls, useMock } from "../mock.ts"
import { DEFAULTS, encodeWorldCode } from "../../worldgen/config.ts"
import { mapUv } from "../../game/map/data.ts"

/** The real HUD, fed through the real store by the mock engine. */
const bridge = useMock()
const store = useStore()
const creativePreview = new URLSearchParams(location.search).has("creative")
const code = encodeWorldCode({ ...DEFAULTS, seed: "pine ridge" })
const exploredBytes = new Uint8Array(256 * 256 / 8)
for (let z = 96; z < 160; z++) for (let x = 96; x < 160; x++) {
    if ((x - 128) ** 2 + (z - 128) ** 2 > 32 ** 2) continue
    const bit = z * 256 + x
    exploredBytes[bit >> 3]! |= 1 << (bit & 7)
}
const explored = btoa(String.fromCharCode(...exploredBytes))

const live = reactive({
    hp: 17, hpMax: 20, breath: 7, breathMax: 10, selected: 2, size: 9,
    visible: true, crosshair: true, wear: 30000, altitude: 120, trajectory: true,
    building: false, world: false, cursor: false,
})

function push() {
    bridge.emit({ kind: "mode", mode: "game" })
    bridge.emit({ kind: "state", section: "hud", value: { visible: live.visible, crosshair: live.crosshair, hotbar: true, health: true, breath: true } })
    bridge.emit({ kind: "state", section: "player", value: { hp: live.hp, hpMax: live.hpMax, breath: live.breath, breathMax: live.breathMax } })
    bridge.emit({ kind: "state", section: "hotbar", value: { size: live.size, selected: live.selected } })
    bridge.emit({ kind: "state", section: "tools", value: live.building ? {
        out: true, tool: "cylinder", size: 24, mirrored: false, focused: true, cursor: live.cursor, naming: false,
        brush: "add", creating: !live.world, partJoint: "fixed",
    } : { out: false } })
    bridge.emit({ kind: "state", section: "library", value: ["small cart", "Spoked wheel"] })
    bridge.emit({ kind: "ui", key: "world_controls", value: mockWorldControls() })
    bridge.emit({ kind: "ui", key: "map", value: {
        code, size: DEFAULTS.size, radius: DEFAULTS.size / (2 * Math.PI), sea: 1,
        star: DEFAULTS.star, tilt: DEFAULTS.tilt,
        mode: creativePreview ? "creative" : "survival", explored, grid: 256,
    } })
    bridge.emit({ kind: "ui", key: "map_position", value: { x: 384, z: -128, date: 0.25 } })
    const mapPose = mapUv(384, -128, DEFAULTS.size)
    bridge.emit({ kind: "state", section: "mapPose", value: { ...mapPose, heading: 35, height: live.altitude + 1 } })
    bridge.emit({ kind: "ui", key: "map_trajectory", value: live.trajectory ? {
        points: Array.from({ length: 129 }, (_, i): [number, number, number] => {
            const turn = i / 128
            return [(mapPose.u + turn) % 1, mapPose.v + 0.035 * Math.sin(turn * 2 * Math.PI), live.altitude + 24 * Math.sin(turn * 2 * Math.PI)]
        }),
        ending: "time", seconds: 420,
    } : null })
    bridge.emit({
        kind: "state",
        section: "inventory",
        value: {
            main: {
                width: 8,
                stacks: [
                    { item: "tf_world:grass", count: 64, wear: 0 },
                    { item: "tf_world:dirt", count: 12, wear: 0 },
                    { item: "tf_tools:pick", count: 1, wear: live.wear },
                    { item: "tf_world:stone", count: 1, wear: 0 },
                    null,
                    { item: "tf_world:sand", count: 33, wear: 0 },
                    { item: "unknown:thing", count: 2, wear: 0 },
                    null, null, null,
                ],
            },
        },
    })
}

watch(live, push)
onMounted(() => {
    push()
    const screen = new URLSearchParams(location.search).get("screen")
    if (screen === "map" || screen === "pause") bridge.emit({ kind: "screen", screen })
})
</script>

<template>
    <div class="page">
        <div class="world">
            <div class="sky" />
            <Hud />
            <PauseScreen v-if="store.screen === 'pause'" />
            <MapScreen v-else-if="store.screen === 'map'" />
        </div>
        <aside>
            <Section title="Player">
                <Slider v-model="live.hp" label="HP" :min="0" :max="live.hpMax" :step="1" />
                <Slider v-model="live.hpMax" label="Max HP" :min="2" :max="40" :step="2" />
                <Slider v-model="live.breath" label="Breath" :min="0" :max="live.breathMax" :step="1" />
            </Section>
            <Section title="Hotbar">
                <Slider v-model="live.selected" label="Selected" :min="0" :max="live.size - 1" :step="1" />
                <Slider v-model="live.size" label="Size" :min="1" :max="10" :step="1" />
                <Slider v-model="live.wear" label="Pickaxe wear" :min="0" :max="65535" :step="500" />
            </Section>
            <Section title="Building">
                <Toggle v-model="live.building" label="Build mode (Tab)" />
                <Toggle v-model="live.world" label="Alt held: into the world" />
                <Toggle v-model="live.cursor" label="Cursor free (C)" />
            </Section>
            <Section title="Screens">
                <Toggle :model-value="store.screen === 'pause'" label="Pause screen (Esc)"
                    @update:model-value="bridge.emit({ kind: 'screen', screen: $event ? 'pause' : null })" />
                <Toggle :model-value="store.screen === 'map'" label="World map (V)"
                    @update:model-value="bridge.emit({ kind: 'screen', screen: $event ? 'map' : null })" />
            </Section>
            <Section title="Globe flight">
                <Slider v-model="live.altitude" label="Altitude" :min="0" :max="1000" :step="10" unit="m" />
                <Toggle v-model="live.trajectory" label="Projected path" />
            </Section>
            <Section title="Flags">
                <Toggle v-model="live.visible" label="HUD visible (F1)" />
                <Toggle v-model="live.crosshair" label="Crosshair" />
            </Section>
        </aside>
    </div>
</template>

<style scoped>
.page { display: grid; grid-template-columns: 1fr 320px; gap: 24px; height: 100%; min-height: 560px; }
.world { position: relative; overflow: hidden; border: var(--b) solid var(--edge); transform: translateZ(0); /* contains the HUD's fixed layer */ }
.sky { position: absolute; inset: 0; background: linear-gradient(180deg, #8ab6f0, #c4dcf5 60%, #5b8f3a 60%, #4c7a31); }
aside { display: flex; flex-direction: column; gap: 24px; }
</style>
