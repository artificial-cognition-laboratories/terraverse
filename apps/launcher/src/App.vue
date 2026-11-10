<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { IconRail } from "@game/kit/index.ts"
import { GEAR, PLANET, STACK } from "./icons.ts"
import Home from "./pages/Home.vue"
import Settings from "./pages/Settings.vue"
import Versions from "./pages/Versions.vue"
import ProfileMenu from "./parts/ProfileMenu.vue"
import ReportDialog from "./parts/ReportDialog.vue"
import { useStore } from "./use.ts"

type Page = "game" | "versions" | "settings"
const PAGES = [
    { id: "game", label: "Game", ...PLANET },
    { id: "versions", label: "Versions", ...STACK },
    { id: "settings", label: "Settings", end: true, ...GEAR },
] as const

const store = useStore()
const { state } = store
// `#versions` or `#settings` opens a page: for links, and for looking at one while building
const opened = location.hash.slice(1)
const page = ref<Page>(PAGES.some(p => p.id === opened) ? (PAGES.find(p => p.id === opened)?.id ?? "game") : "game")
const reporting = ref(false)

const loaded = computed(() => store.account.value && state.settings && state.game && state.installed)
const crash = computed(() => state.game?.state === "crashed" ? state.game : null)
// a refusal shows for a while, then goes; a new one replaces it
let fade: ReturnType<typeof setTimeout> | undefined
watch(() => state.refusal, refusal => {
    clearTimeout(fade)
    if (refusal) fade = setTimeout(() => store.dismissRefusal(), 6000)
})
</script>

<template>
    <div v-if="loaded" class="launcher">
        <IconRail v-model="page" :items="PAGES" label="Launcher">
            <template #foot><ProfileMenu /></template>
        </IconRail>
        <main class="stage">
            <Home v-if="page === 'game'" />
            <Versions v-else-if="page === 'versions'" />
            <Settings v-else @report="reporting = true" />
        </main>

        <ReportDialog v-if="crash" :key="crash.session" :crash="crash" @close="() => {}" />
        <ReportDialog v-else-if="reporting" :crash="null" @close="reporting = false" />

        <div v-if="state.refusal" class="refusal" role="alert">
            <span>{{ state.refusal }}</span>
            <button aria-label="Dismiss" @click="store.dismissRefusal()">✕</button>
        </div>
    </div>
</template>

<style scoped>
.launcher { position: relative; display: grid; grid-template-columns: 76px minmax(0, 1fr); grid-template-rows: minmax(0, 1fr); height: 100%; overflow: hidden; background: var(--stone-0); }
.stage { min-width: 0; min-height: 0; }
.refusal {
    position: absolute; top: 16px; left: 50%; z-index: 20; transform: translateX(-50%); display: flex; align-items: center; gap: 14px;
    max-width: calc(100% - 32px); padding: 10px 14px; border: var(--b) solid var(--edge); background: var(--danger-lo);
    box-shadow: var(--raised), 6px 6px 0 rgb(0 0 0 / 0.35); font-size: 13px;
}
.refusal button { padding: 0 4px; border: 0; background: none; }
</style>
