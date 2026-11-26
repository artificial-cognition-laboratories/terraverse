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
.icon-rail { display: flex; flex-direction: column; gap: 4px; min-width: 0; min-height: 0; overflow-y: auto; padding: 12px 8px; border-right: 1px solid var(--glass-edge); background: var(--side); }
/* a rail too tall for a short window still scrolls, by the wheel: a bar beside its icons would read as a seam */
.icon-rail::-webkit-scrollbar { display: none; }
button { position: relative; display: flex; flex-direction: column; align-items: center; gap: 5px; width: 100%; padding: 10px 2px; border: 1px solid transparent; border-radius: var(--radius); background: none; color: var(--ink-faint); font: 8px var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; transition: background 80ms, color 80ms; }
button:hover { color: var(--ink); background: rgb(231 237 244 / 0.05); }
button.on { border-color: transparent; background: rgb(99 163 233 / 0.07); color: var(--ink); }
/* the chosen page's mark: an accent bar on the rail's outer edge */
button.on::before { content: ""; position: absolute; left: -9px; top: 6px; bottom: 6px; width: var(--b); background: var(--accent-hi); }
button svg { flex: none; }
.push { margin-top: auto; }
.foot { display: grid; place-items: center; padding-top: 6px; }
/* short: icons only, the label stays in the tooltip, so every page fits without scrolling */
@media (max-height: 400px) {
    .icon-rail { gap: 4px; padding: 8px 7px; }
    button { padding: 6px 2px; }
    button span { display: none; }
}
span { max-width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
</style>
