<script setup lang="ts">
import type { Loading } from "../../bridge/index.ts"
import PixelIcon from "../../game/PixelIcon.vue"
import type { Chip } from "../worldIcons.ts"

/**
 * What's chosen in Play, laid over the planet in the room the dock leaves (`left`, page pixels):
 * where it's from, its name large, its chips and a line about it, then its actions (the slot).
 * Entering, the actions stay (the one pressed busy, the slot's to say) and the stage under way
 * is one faint line beside them: no card over the planet.
 */
withDefaults(defineProps<{
    left: number; kicker: string; title: string | null; chips?: readonly Chip[]; meta?: string | null
    entering: Loading | null; problems: readonly string[]
}>(), { chips: () => [], meta: null })
defineEmits<{ dismiss: [] }>()
</script>

<template>
    <section class="stage" :style="{ left: `calc(${left}px + 2.5rem)` }">
        <p v-for="p in problems" :key="p" class="problem" role="alert">
            <span>{{ p }}</span><button type="button" aria-label="Dismiss" @click="$emit('dismiss')">✕</button>
        </p>
        <template v-if="title">
            <p class="kicker"><span aria-hidden="true">»</span> {{ kicker }}</p>
            <h1>{{ title }}</h1>
            <div v-if="chips.length" class="chips">
                <span v-for="chip in chips" :key="chip.label" class="chip" :title="chip.label">
                    <PixelIcon class="chip-icon" :rows="chip.rows" :palette="chip.palette" />{{ chip.text }}
                </span>
            </div>
            <p v-if="meta" class="meta">{{ meta }}</p>
        </template>
        <div class="actions">
            <slot />
            <span v-if="entering" class="under-way" role="status">{{ entering.stages.find((s) => s.state === "running")?.label ?? "Starting" }}…</span>
        </div>
    </section>
</template>

<style scoped>
/* on the night itself, as the website sets words on its sky: a halo of the night round each glyph */
.stage { position: fixed; bottom: 2.5rem; right: 2.5rem; display: grid; justify-items: start; gap: 0.8rem; max-width: 34rem; pointer-events: auto;
    text-shadow: 0 0 3px var(--night-0), 0 0 6px var(--night-0), 0 0 10px var(--night-0); }
.kicker { margin: 0; color: var(--accent-hi); font: 0.72rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
h1 { margin: 0; font: 400 2.4rem/1.1 var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; overflow-wrap: anywhere; }
.chips { display: flex; flex-wrap: wrap; gap: 0.3rem 1rem; }
.chip { display: inline-flex; align-items: center; gap: 0.4rem; color: var(--ink-dim); font: 0.68rem var(--font-display); text-transform: uppercase; }
.chip-icon { width: 14px; height: 14px; }
.meta { margin: 0; color: var(--ink-faint); font-size: 0.78rem; }
.actions { display: flex; align-items: center; gap: 0.6rem; margin-top: 0.6rem; }
.under-way { margin-left: 0.6rem; color: var(--ink-faint); font: 0.65rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; white-space: nowrap; }
.problem { display: flex; gap: 0.75rem; align-items: start; margin: 0; max-width: 26rem; padding: 0.6rem 0.75rem; border: 1px solid rgb(248 81 73 / 0.4); background: var(--glass-strong); color: var(--danger); font-size: 0.78rem; text-shadow: none; }
.problem button { border: 0; background: none; padding: 0; color: var(--ink-dim); }
</style>
