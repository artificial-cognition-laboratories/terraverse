<script setup lang="ts">
import { computed, onMounted, watch } from "vue"
import Corner from "../account/Corner.vue"
import ChatFeed from "./chat/ChatFeed.vue"
import { useBridge } from "../bridge/use.ts"
import Crosshair from "./Crosshair.vue"
import Hotbar from "./Hotbar.vue"
import StatBar from "./StatBar.vue"
import ActionRail from "./ActionRail.vue"
import ShapeBar from "./ShapeBar.vue"
import ToolRail from "./ToolRail.vue"
import TravelScreen from "./TravelScreen.vue"
import { useStore } from "./store.ts"
import { worldOf } from "./map/bodies.ts"
import { mapKey, worldMapData } from "./map/data.ts"
import MapWidget from "./widgets/MapWidget.vue"
import { loadWidgetSettings } from "./widgets/config.ts"
import WikiButton from "./widgets/WikiButton.vue"

const store = useStore()
const bridge = useBridge()
onMounted(() => { void loadWidgetSettings(bridge) })
// the map's slow answers asked for as soon as the world's known, so it opens at once
const mapData = worldMapData()
// (by which world: the map's value changes as the player explores, and the screen charts other bodies itself)
watch(() => store.ui.map ? mapKey(store.ui.map) : null, () => {
    const map = store.ui.map
    if (!map) return
    void worldOf(bridge, map.code)
    void mapData.ensure(bridge, map)
}, { immediate: true })
const hud = computed(() => store.state.hud)
const player = computed(() => store.state.player)
// pause keeps the world's HUD up, the mouse free over it; another screen covers it
const paused = computed(() => store.screen === "pause")
const covered = computed(() => store.screen !== null && !paused.value)
const tools = computed(() => store.state.tools)
const library = computed(() => store.state.library)
const hotbar = computed(() => {
    const bar = store.state.hotbar
    const main = store.state.inventory?.main
    if (!bar || !main) return null
    return { stacks: main.stacks.slice(0, bar.size), selected: bar.selected }
})
</script>

<template>
    <div v-if="hud?.visible" class="hud">
        <!-- aiming only: any screen (pause too) has the mouse -->
        <Crosshair v-if="hud.crosshair && store.screen === null" />
        <ToolRail v-if="tools?.out && !covered" :tools="tools" :free="tools.cursor || paused" />
        <ActionRail v-if="tools?.out && library && !covered" :tools="tools" :library="library" :free="tools.cursor || paused" />
        <div class="bottom under-full-screen">
            <div v-if="player" class="stats">
                <StatBar v-if="hud.health" kind="heart" :value="player.hp" :max="player.hpMax" />
                <StatBar v-if="hud.breath && player.breath < player.breathMax" kind="bubble"
                    :value="player.breath" :max="player.breathMax" reverse />
            </div>
            <ShapeBar v-if="tools?.out && !covered" :tools="tools" :free="tools.cursor || paused" />
            <!-- under the inventory, which draws its own exactly here: hidden, not gone, so nothing
                 above it moves, and not two strips of glass stacked darker than the rest -->
            <Hotbar v-if="hud.hotbar && hotbar" :stacks="hotbar.stacks" :selected="hotbar.selected"
                :class="{ under: store.screen === 'inventory' }"
                :free="!!tools?.out && tools.cursor || paused" />
        </div>
    </div>
    <!-- above any screen that leaves them (pause, the inventory, the palette): their own layer, not
         the HUD's, which a screen's backdrop covers -->
    <div v-if="hud?.visible && (!covered || store.screen === 'inventory' || store.screen === 'palette' || store.screen === 'online')" class="corner-icons under-full-screen">
        <WikiButton />
        <!-- its popover is the online screen: the icon stays over it, and closes it again -->
        <Corner @open="store.screen === 'online' ? bridge.send('screen.close', {}) : bridge.send('screen.open', { name: 'online' })" />
    </div>
    <ChatFeed v-if="hud?.visible" class="under-full-screen" />
    <MapWidget />
    <TravelScreen />
</template>

<style scoped>
.hud {
    /* One GUI pixel: a fixed share of the page's type, so the HUD grows with the interface's
       scale as every screen does (the shell's zoom, host/ui_scale.h), never in steps of its own */
    --u: 0.125rem;
    position: fixed;
    inset: 0;
    pointer-events: none;
}

/* one row from the corner in: whichever are there sit together at its end */
.corner-icons { position: fixed; top: 20px; right: 20px; z-index: 10; display: flex; justify-content: flex-end; align-items: flex-start; gap: 10px; }
.corner-icons { pointer-events: none; }
.corner-icons > * { pointer-events: auto; }
.corner-icons > .corner { position: relative; top: auto; right: auto; }

.bottom {
    position: absolute;
    left: 50%;
    bottom: 1rem;
    transform: translateX(-50%);
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
}
.under { visibility: hidden; }

.stats {
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    padding: 0 var(--u);
}
</style>

<style>
/* a full screen (kit/Screen) over the world, however it's opened (the wiki is a screen of its own,
   settings a view inside pause): the HUD's hotbar and corner icons aren't left showing through it */
body:has([data-full-screen]) .under-full-screen { visibility: hidden; }
</style>
