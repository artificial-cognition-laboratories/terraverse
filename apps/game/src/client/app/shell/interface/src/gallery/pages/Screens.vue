<script setup lang="ts">
import { ref } from "vue"
import { Segmented } from "../../kit/index.ts"
import MainMenu from "../../menu/MainMenu.vue"
import SettingsPanel from "../../settings/SettingsPanel.vue"
import { useStore } from "../../game/store.ts"

/** Whole screens against the mock engine: click through them as in the game. */
const screen = ref<"menu" | "settings">("menu")
const error = ref<string | null>(null)
// Play enters a world as the engine would tell it, and comes back with the gallery's error
const store = useStore()
const query = ref("")
</script>

<template>
    <div class="page">
        <div class="bar">
            <Segmented v-model="screen" :options="[{ value: 'menu', label: 'Main menu' }, { value: 'settings', label: 'Settings panel' }]" />
            <button class="err" @click="error = error ? null : 'Connection lost: server closed.'">toggle session error</button>
        </div>
        <div class="frame">
            <MainMenu v-if="screen === 'menu'" :error="error ?? store.error" :entering="store.mode === 'entering' ? store.entering : null"
                :play-music="false" @dismiss="error = null; store.error = null" />
            <div v-else class="pad">
                <SettingsPanel v-model:query="query" />
            </div>
        </div>
    </div>
</template>

<style scoped>
.page { display: flex; flex-direction: column; gap: 14px; height: 100%; }
.bar { display: flex; gap: 16px; align-items: center; }
.bar :deep(.segmented) { width: 360px; }
.err { border: 0; background: none; color: var(--accent-hi); font-size: 12px; }
.frame { flex: 1; min-height: 640px; border: var(--b) solid var(--edge); overflow: hidden; background: #060d16; }
.frame :deep(.create-workbench) { width: min(1400px, 100%); }
.pad { padding: 24px; height: 100%; background: var(--stone-1); }
.pad :deep(.settings-workbench) { width: min(1200px, 100%); height: min(760px, 100%); margin: auto; }
</style>
