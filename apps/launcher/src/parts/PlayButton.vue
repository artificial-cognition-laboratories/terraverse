<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from "vue"
import { Button } from "@game/kit/index.ts"
import type { Version } from "../backend/index.ts"
import { bytes, since, useStore } from "../use.ts"

/** One button that follows the version's standing: install (update, when an older one is installed), download, play, playing. */
const props = defineProps<{ version: Version }>()
const store = useStore()
const { state } = store

const standing = computed(() => store.standing(props.version.id))
const updating = computed(() => !!state.installed?.length)
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
        <!-- installing: the button becomes its own bar, filling with the ocean -->
        <div v-else-if="standing.state === 'downloading'" class="installing" role="progressbar"
            :aria-valuenow="Math.round(standing.progress.received / standing.progress.total * 100)" aria-valuemin="0" aria-valuemax="100">
            <span class="fill" :style="{ width: `${standing.progress.received / standing.progress.total * 100}%` }" />
            <span class="what"><strong>Installing</strong><small class="pct">{{ Math.floor(standing.progress.received / standing.progress.total * 100) }}%</small><small class="of">{{ bytes(standing.progress.received) }} of {{ bytes(standing.progress.total) }}</small></span>
            <button class="cancel" aria-label="Cancel the install" title="Cancel" @click="store.cancel(version.id)">✕</button>
        </div>
        <Button v-else-if="standing.state === 'installed'" variant="primary" size="sm" class="go" @click="store.play(version.id)">Play »</Button>
        <Button v-else variant="primary" size="sm" class="go" @click="store.install(version.id)">
            {{ standing.state === "failed" ? "Retry" : updating ? "Update" : "Install" }}
            <small class="size">{{ bytes(version.size) }}</small>
        </Button>
    </div>
</template>

<style scoped>
/* the game's menu buttons (kit Button, sm): Play is the primary; installing, the same block fills */
.play-button { container-type: inline-size; display: flex; height: 2.1rem; min-width: 0; }
.go { flex: 1; }
.size { font: 0.65rem var(--font); letter-spacing: 0; text-transform: none; opacity: 0.7; }

.installing, .playing { position: relative; border: 1px solid var(--edge-lit-hi); background: var(--glass); backdrop-filter: blur(8px); flex: 1; display: flex; align-items: center; gap: 12px; min-width: 0; padding: 0 4px 0 12px; overflow: hidden; }
.fill { position: absolute; inset: 0 auto 0 0; background: var(--glass-lit); border-right: 1px solid var(--edge-lit-hi); transition: width 0.25s linear; }
.what, .playing span:last-child { position: relative; flex: 1; display: flex; align-items: baseline; gap: 10px; min-width: 0; overflow: hidden; }
strong { font: 0.65rem var(--font-display); font-weight: 400; text-transform: uppercase; letter-spacing: 0.08em; color: #fff; white-space: nowrap; }
small { font: 0.7rem var(--font); color: var(--ink-dim); white-space: nowrap; }
.cancel { position: relative; width: 26px; height: 26px; border: 1px solid transparent; background: none; color: var(--ink-dim); }
.cancel:hover { border-color: var(--glass-edge); color: #fff; }
.dot { width: 8px; height: 8px; flex: none; background: var(--accent-hi); box-shadow: 0 0 0 3px var(--accent-glow); }
/* the amount where there's room for it (Versions), the share where there isn't (the landing's group) */
.pct { display: none; }
@container (max-width: 250px) { .of { display: none; } .pct { display: inline; } }
@media (max-width: 520px) { .size { display: none; } }
</style>
