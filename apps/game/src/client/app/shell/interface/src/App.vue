<script setup lang="ts">
import { defineAsyncComponent, ref, watch } from "vue"
import { useBridge } from "./bridge/use.ts"
import Hud from "./game/Hud.vue"
import InventoryScreen from "./game/inventory/InventoryScreen.vue"
import PauseScreen from "./game/PauseScreen.vue"
import MapScreen from "./game/map/MapScreen.vue"
import PaletteScreen from "./game/palette/PaletteScreen.vue"
import CutScreen from "./game/cut/CutScreen.vue"
import DigScreen from "./game/dig/DigScreen.vue"
import { useStore } from "./game/store.ts"
import MainMenu from "./menu/MainMenu.vue"
import InviteToast from "./social/InviteToast.vue"
import Friends from "./friends/Friends.vue"
import { Spinner } from "./kit/index.ts"
import type { LibraryOpen } from "./menu/library/icons.ts"

// the wiki brings three.js, so it and the library are their own chunks, kept off the first paint;
// fetched once the page is idle, so neither waits on its code the first time it's opened
const loadWiki = () => import("./wiki/WikiScreen.vue")
const loadLibrary = () => import("./menu/library/Library.vue")
const WikiScreen = defineAsyncComponent(loadWiki)
const Library = defineAsyncComponent(loadLibrary)
requestIdleCallback(() => { void loadWiki(); void loadLibrary() }, { timeout: 5000 })
const bridge = useBridge()
const store = useStore()
// asked for from the popover (a friend's page, your account): the library opens on it, and forgets it once left
const libraryOpen = ref<LibraryOpen | null>(null)
watch(() => store.screen, (screen) => { if (screen !== "library") libraryOpen.value = null })
function openLibrary(to: LibraryOpen) {
    libraryOpen.value = to
    void bridge.send("screen.open", { name: "library" })
}
</script>

<template>
    <p v-if="!bridge.connected" class="detached">Not running inside the engine.</p>
    <!-- one branch for both: the menu stays as it was while its world is entered, and if it isn't -->
    <MainMenu v-else-if="store.mode === 'menu' || store.mode === 'entering'" :error="store.error" :entering="store.mode === 'entering' ? store.entering : null" @dismiss="store.error = null" />
    <template v-else-if="store.mode === 'game'">
        <Hud />
        <PauseScreen v-if="store.screen === 'pause'" />
        <MapScreen v-else-if="store.screen === 'map'" />
        <InventoryScreen v-else-if="store.screen === 'inventory'" />
        <PaletteScreen v-else-if="store.screen === 'palette'" />
        <CutScreen v-else-if="store.screen === 'cut'" />
        <DigScreen v-else-if="store.screen === 'dig'" />
        <div v-else-if="store.screen === 'wiki'" class="whole"><WikiScreen place="game" @close="bridge.send('screen.close', {})" /></div>
        <!-- over the world, frosted as the menu's scene is; the search waits to be clicked, so L closes it again -->
        <div v-else-if="store.screen === 'library'" class="whole"><Library :autofocus="false" :open="libraryOpen" @close="bridge.send('screen.close', {})" /></div>
        <!-- the account corner's popover, over the world as in the menu; the full Online screen waits to be re-homed -->
        <Friends v-else-if="store.screen === 'online'" :account="store.account" :social="store.social" @close="bridge.send('screen.close', {})" @library="openLibrary" />
    </template>
    <!-- over the world's last frame, held frosted by the shell while it's saved (host/frame_blur.h: hold) -->
    <div v-else-if="store.mode === 'leaving'" class="leaving"><p><Spinner label="Saving the world" /> Saving the world</p></div>
    <InviteToast v-if="bridge.connected" />
</template>

<style scoped>
.whole { position: fixed; inset: 0; }
/* dim enough to read over, solid enough (≥ 40%) that the shell frosts the world under it */
.leaving { position: fixed; inset: 0; display: grid; place-items: center; background: rgb(4 7 11 / 0.55); }
/* no card: the words alone on the frosted world */
.leaving p { display: flex; gap: 0.75rem; align-items: center; margin: 0; color: var(--ink); font: 0.95rem var(--font-sans); text-shadow: 0 1px 2px rgb(0 0 0 / 0.6); }
.detached { padding: 24px; color: var(--ink-dim); background: var(--stone-0); height: 100%; margin: 0; }
</style>
