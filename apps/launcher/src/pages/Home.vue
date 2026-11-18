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

const running = computed(() => state.game?.state === "running")

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
            <div v-if="store.newerThanPinned.value" class="launcher-update">
                <span>Terraverse {{ store.newerThanPinned.value.id }} is out. You're pinned to {{ store.target.value?.id }}.</span>
                <Button size="sm" @click="store.pin(null)">Play the latest</Button>
            </div>
            <div v-if="store.previous.value && !running" class="launcher-update">
                <span>Update to {{ store.target.value?.id }}, or keep playing {{ store.previous.value.id }} for now.</span>
                <Button size="sm" @click="store.play(store.previous.value.id)">Play {{ store.previous.value.id }}</Button>
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

/* the title scales with the window both ways, so a small window still shows it whole, above the dock */
header { position: absolute; top: clamp(16px, 9vh, 56px); left: clamp(16px, 6vw, 56px); right: 16px; }
h1 {
    margin: 0; font: 400 clamp(20px, min(6.4vw, 11vh), 96px) / 1 var(--font-display); text-transform: uppercase; letter-spacing: 0.04em;
    white-space: nowrap; color: #eef3f6;
    /* the extrusion in em, so it keeps its depth as the title shrinks (2, 4, 6, 8 and 14 px at 61 px) */
    text-shadow: 0.033em 0.033em 0 #1c6f96, 0.066em 0.066em 0 #13536f, 0.1em 0.1em 0 #0a3a52, 0.13em 0.13em 0 #062636, 0.23em 0.23em 0 rgb(0 0 0 / 0.4);
}
.tagline { margin: clamp(10px, 4vh, 24px) 0 0 4px; font: 10px var(--font); letter-spacing: 0.32em; text-transform: uppercase; color: var(--ink-dim); }

/* what you do here, floating at the bottom right: no bar */
.dock { position: absolute; right: 14px; bottom: 14px; display: flex; flex-direction: column; align-items: flex-end; gap: 10px; max-width: calc(100% - 28px); }
.dock :deep(.notice) { max-width: 560px; background-color: rgb(21 24 27 / 0.92); }
.controls { display: flex; align-items: stretch; gap: 10px; max-width: 100%; }
.play { width: 300px; min-width: 0; }
.launcher-update { display: flex; flex-wrap: wrap; align-items: center; justify-content: flex-end; gap: 6px 12px; padding: 6px 6px 6px 12px; border-left: 4px solid var(--accent); background: rgb(21 24 27 / 0.92); font-size: 12px; color: var(--ink-dim); }
.launcher-update span { flex: 1 1 200px; }
.none { display: flex; align-items: center; gap: 12px; padding: 0 14px; color: var(--ink-dim); font-size: 12px; background: rgb(21 24 27 / 0.92); }
/* narrow: the dock takes the stage's width, the picker gives Play the room */
@media (max-width: 640px) {
    .dock { left: 14px; align-items: stretch; }
    .controls { width: 100%; }
    .play { flex: 1; width: auto; }
}
@media (max-height: 380px) { .tagline { display: none; } }
</style>
