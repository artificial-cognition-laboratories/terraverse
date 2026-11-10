<script setup lang="ts">
/**
 * Work done in stages, one line each: what it is, where it's got, and how long
 * it took. A stage read from what was kept says so rather than a time.
 */
import type { LoadingStage } from "../bridge"

defineProps<{ stages: LoadingStage[] }>()

const MARKS: Record<LoadingStage["state"], string> = { waiting: "·", running: "»", done: "✓", kept: "✓" }

function took(stage: LoadingStage): string {
    if (stage.state === "waiting") return ""
    if (stage.state === "kept") return "kept"
    const time = `${stage.seconds.toFixed(1)} s`
    return stage.state === "running" && stage.share !== null ? `${Math.round(stage.share * 100)}% · ${time}` : time
}
</script>

<template>
    <ol class="stages">
        <li v-for="stage in stages" :key="stage.id" :class="stage.state">
            <span class="mark" aria-hidden="true">{{ MARKS[stage.state] }}</span>
            <span class="label">{{ stage.label }}</span>
            <span class="took">{{ took(stage) }}</span>
        </li>
    </ol>
</template>

<style scoped>
.stages { display: grid; gap: 2px; margin: 0; padding: 0; list-style: none; font: 10px/16px var(--font-display); text-transform: uppercase; }
li { display: grid; grid-template-columns: 12px minmax(0, 1fr) auto; gap: 6px; color: var(--ink-dim); }
.label { min-width: 0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.took { font-family: var(--font); text-transform: none; white-space: nowrap; }
.waiting { color: var(--ink-faint); }
.running { color: var(--ink); }
.running .mark { color: var(--accent); }
.kept .took { color: var(--ink-faint); }
</style>
