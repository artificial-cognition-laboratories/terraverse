<script setup lang="ts" generic="T extends string">
import PixelIcon from "../game/PixelIcon.vue"

/** A column of pages. Items marked `end` sit at its foot, above the `foot` slot (an account button). */
defineProps<{
    items: readonly { id: T; label: string; shortLabel?: string; end?: boolean; rows: readonly string[]; palette: Record<string, string> }[]
    label: string
}>()
const model = defineModel<T>({ required: true })
</script>

<template>
    <nav class="icon-rail" :aria-label="label">
        <button v-for="(item, i) in items" :key="item.id"
            :class="{ on: model === item.id, push: item.end && !items[i - 1]?.end }"
            :title="item.label" :aria-label="item.label"
            :aria-current="model === item.id ? 'page' : undefined" @click="model = item.id">
            <PixelIcon :rows="item.rows" :palette="item.palette" style="--u: 3px" />
            <span>{{ item.shortLabel ?? item.label }}</span>
        </button>
        <div v-if="$slots.foot" class="foot" :class="{ push: !items.some(item => item.end) }"><slot name="foot" /></div>
    </nav>
</template>

<style scoped>
.icon-rail { display: flex; flex-direction: column; gap: 6px; min-width: 0; min-height: 0; overflow-y: auto; padding: 14px 7px; border-right: var(--b) solid var(--edge); background: var(--stone-2); }
button { display: flex; flex-direction: column; align-items: center; gap: 4px; width: 100%; padding: 8px 2px; border: var(--b) solid transparent; background: none; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
button:hover { color: var(--ink); background: var(--stone-3); }
button.on { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
button svg { flex: none; }
.push { margin-top: auto; }
.foot { display: grid; place-items: center; padding-top: 6px; }
span { max-width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
</style>
