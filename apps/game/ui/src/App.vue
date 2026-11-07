<script setup lang="ts">
import { defineAsyncComponent } from "vue"
import { useBridge } from "./bridge/use.ts"
import Hud from "./game/Hud.vue"
import InventoryScreen from "./game/inventory/InventoryScreen.vue"
import PauseScreen from "./game/PauseScreen.vue"
import MapScreen from "./game/map/MapScreen.vue"
import { useStore } from "./game/store.ts"
import MakingScreen from "./game/making/MakingScreen.vue"
import { openMaking } from "./game/making/open.ts"
import MainMenu from "./menu/MainMenu.vue"
import InviteToast from "./social/InviteToast.vue"

// the wiki brings three.js: loaded when first opened
const WikiScreen = defineAsyncComponent(() => import("./wiki/WikiScreen.vue"))
const bridge = useBridge()
const store = useStore()
openMaking({ bridge: bridge, store: store })
</script>

<template>
    <p v-if="!bridge.connected" class="detached">Not running inside the engine.</p>
    <MainMenu v-else-if="store.mode === 'menu'" :error="store.error" @dismiss="store.error = null" />
    <template v-else-if="store.mode === 'game'">
        <Hud />
        <PauseScreen v-if="store.screen === 'pause'" />
        <MapScreen v-else-if="store.screen === 'map'" />
        <InventoryScreen v-else-if="store.screen === 'inventory'" />
        <MakingScreen v-else-if="store.screen === 'making'" />
        <div v-else-if="store.screen === 'wiki'" class="centred"><WikiScreen place="game" @close="bridge.send('screen.close', {})" /></div>
    </template>
    <InviteToast v-if="bridge.connected" />
</template>

<style scoped>
.centred { position: fixed; inset: 0; display: grid; place-items: center; }
.detached { padding: 24px; color: var(--ink-dim); background: var(--stone-0); height: 100%; margin: 0; }
</style>
