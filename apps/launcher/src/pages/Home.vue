<script setup lang="ts">
import { computed } from "vue"
import { Button, Notice } from "@game/kit/index.ts"
import Backdrop from "../parts/Backdrop.vue"
import PlayButton from "../parts/PlayButton.vue"
import VersionPicker from "../parts/VersionPicker.vue"
import { useStore } from "../use.ts"

const store = useStore()
const { state } = store
const TITLE = "Terraverse"

const failure = computed(() => {
    const target = store.target.value
    const standing = target ? store.standing(target.id) : null
    return standing?.state === "failed" ? standing.reason : null
})
</script>

<template>
    <div class="home">
        <Backdrop />
        <div class="shade" aria-hidden="true" />

        <header>
            <!-- the main menu's title, still: the same word, the same extrusion -->
            <h1>{{ TITLE }}</h1>
            <p class="tagline">a whole planet. every block of it.</p>
        </header>

        <div class="dock">
            <Notice v-if="state.offline" tone="warn">{{ state.offline }} Showing what was known when last online.</Notice>
            <Notice v-if="failure" tone="danger">{{ failure }}</Notice>
            <div v-if="state.launcherUpdate" class="launcher-update">
                <span>Launcher {{ state.launcherUpdate.version }} is ready.</span>
                <Button size="sm" @click="store.updateLauncher()">Restart to update</Button>
            </div>
            <div class="controls">
                <VersionPicker />
                <div v-if="store.target.value" class="play"><PlayButton :version="store.target.value" /></div>
                <div v-else-if="state.manifest" class="none">
                    No versions for this platform yet.
                    <Button size="sm" @click="store.refreshManifest()">Check again</Button>
                </div>
            </div>
        </div>
    </div>
</template>

<style scoped>
.home { position: relative; height: 100%; overflow: hidden; }
.shade { position: absolute; inset: 0; pointer-events: none;
    background: linear-gradient(90deg, rgb(3 8 15 / 0.55), transparent 55%), linear-gradient(0deg, rgb(3 8 15 / 0.6), transparent 35%); }

header { position: absolute; top: 56px; left: 56px; }
h1 {
    margin: 0; font: 400 clamp(44px, 6.4vw, 96px) / 1 var(--font-display); text-transform: uppercase; letter-spacing: 0.04em;
    white-space: nowrap; color: #eef3f6;
    text-shadow: 2px 2px 0 #1c6f96, 4px 4px 0 #13536f, 6px 6px 0 #0a3a52, 8px 8px 0 #062636, 14px 14px 0 rgb(0 0 0 / 0.4);
}
.tagline { margin: 24px 0 0 4px; font: 10px var(--font); letter-spacing: 0.32em; text-transform: uppercase; color: var(--ink-dim); }

/* what you do here, floating at the bottom right: no bar */
.dock { position: absolute; right: 14px; bottom: 14px; display: flex; flex-direction: column; align-items: flex-end; gap: 10px; max-width: calc(100% - 28px); }
.dock :deep(.notice) { max-width: 560px; background-color: rgb(21 24 27 / 0.92); }
.controls { display: flex; align-items: stretch; gap: 10px; }
.play { width: 300px; }
.launcher-update { display: flex; align-items: center; gap: 12px; padding: 6px 6px 6px 12px; border-left: 4px solid var(--accent); background: rgb(21 24 27 / 0.92); font-size: 12px; color: var(--ink-dim); }
.none { display: flex; align-items: center; gap: 12px; padding: 0 14px; color: var(--ink-dim); font-size: 12px; background: rgb(21 24 27 / 0.92); }
</style>
