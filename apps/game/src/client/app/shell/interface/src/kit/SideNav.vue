<script setup lang="ts" generic="T extends string">
import PixelIcon from "../game/PixelIcon.vue"
import type { Icon } from "./icons.ts"

/** Stacked choices at the top of a side card: a glyph, a label, a count; the chosen one lit along its edge. */
defineProps<{ items: readonly { id: T; label: string; icon: Icon; count?: number | null }[] }>()
const model = defineModel<T>({ required: true })
</script>

<template>
    <nav class="side-nav flush">
        <button v-for="item in items" :key="item.id" type="button" :class="{ on: model === item.id }"
            :aria-pressed="model === item.id" @click="model = item.id">
            <PixelIcon :rows="item.icon.rows" :palette="item.icon.palette" style="--u: 1.6px" />
            <span class="label">{{ item.label }}</span>
            <span v-if="item.count != null" class="count">{{ item.count }}</span>
        </button>
    </nav>
</template>

<style scoped>
.side-nav { display: grid; gap: 0.15rem; padding: 0.5rem; }
button { display: flex; gap: 0.75rem; align-items: center; padding: 0.6rem 0.65rem; border: 0; background: none; box-shadow: inset 2px 0 0 transparent; color: var(--ink-dim); font: 0.66rem var(--font-display); text-transform: uppercase; text-align: left; }
button:hover { color: var(--ink); }
button.on { color: var(--ink); background: rgb(99 163 233 / 0.07); box-shadow: inset 2px 0 0 var(--accent-hi); }
.label { flex: 1; }
.count { color: var(--ink-faint); font: 0.68rem var(--font); }
</style>
