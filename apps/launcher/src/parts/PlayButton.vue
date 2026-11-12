<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from "vue"
import { Button, Progress } from "@game/kit/index.ts"
import type { Version } from "../backend/index.ts"
import { bytes, since, useStore } from "../use.ts"

/** One button that follows the version's standing: install, download, play, playing. */
const props = defineProps<{ version: Version }>()
const store = useStore()
const { state } = store

const standing = computed(() => store.standing(props.version.id))
const running = computed(() => state.game?.state === "running" ? state.game : null)

// the time played, ticking by the minute: no faster repaint than that
const now = ref(Date.now())
const clock = setInterval(() => { now.value = Date.now() }, 30_000)
onBeforeUnmount(() => clearInterval(clock))
</script>

<template>
    <div class="play-button">
        <div v-if="running" class="playing">
            <span class="dot" aria-hidden="true" />
            <span><strong>Playing {{ running.version }}</strong><small>{{ since(running.since, now) }}</small></span>
        </div>
        <div v-else-if="standing.state === 'downloading'" class="downloading">
            <Progress :value="standing.progress.received / standing.progress.total"
                :label="`Installing · ${bytes(standing.progress.received)} of ${bytes(standing.progress.total)}`" />
            <Button size="sm" @click="store.cancel(version.id)">Cancel</Button>
        </div>
        <Button v-else-if="standing.state === 'installed'" variant="primary" size="lg" class="go" @click="store.play(version.id)">
            Play
        </Button>
        <Button v-else variant="primary" size="lg" class="go" @click="store.install(version.id)">
            {{ standing.state === "failed" ? "Retry" : "Install" }}
            <small class="size">{{ bytes(version.size) }}</small>
        </Button>
    </div>
</template>

<style scoped>
.play-button { display: flex; align-items: stretch; width: 100%; min-width: 0; height: var(--play-height, 56px); }
.go { flex: 1; height: 100%; font-size: 20px; }
.size { font: 10px var(--font); letter-spacing: 0; text-transform: none; opacity: 0.75; }
.downloading { flex: 1; display: flex; align-items: center; gap: 10px; min-width: 0; }
.downloading > :first-child { flex: 1; min-width: 0; }
.playing {
    flex: 1; display: flex; align-items: center; gap: 14px; padding: 0 18px;
    border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk);
}
.playing span:last-child { display: flex; flex-direction: column; }
.playing strong { font: 13px var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; }
.playing small { font-size: 11px; color: var(--ink-dim); }
.dot { width: 10px; height: 10px; background: var(--accent-hi); box-shadow: 0 0 0 3px var(--accent-glow); }
</style>
