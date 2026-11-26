<script setup lang="ts">
/**
 * The game's square icon button: a square of glass, its edge lit in the accent on
 * hover, filled with the deep ocean when on. Its
 * label is a tag beside it on hover (`side`); the slot holds the icon and
 * any marks on it (a badge, a size).
 */
withDefaults(defineProps<{ label: string; on?: boolean; disabled?: boolean; side?: "left" | "right" | "none" }>(), {
    on: false,
    disabled: false,
    side: "none",
})
</script>

<template>
    <button class="icon-button" :class="[side, { on: on }]" :disabled="disabled" :aria-label="label"
        :aria-pressed="on">
        <slot />
        <span v-if="side !== 'none'" class="tag">{{ label }}</span>
    </button>
</template>

<style scoped>
.icon-button {
    position: relative; display: grid; place-items: center; width: 44px; height: 44px; padding: 0;
    color: var(--ink-dim); background: var(--glass); backdrop-filter: blur(8px); border: 1px solid var(--glass-edge); border-radius: var(--radius);
}
/* as Button: the edge lit faintly, the icon brighter, the glass unchanged, at once */
.icon-button:hover:not(:disabled) { color: var(--ink); border-color: var(--edge-lit); }
.icon-button.on { color: #fff; background: var(--glass-lit); border-color: var(--edge-lit-hi); }
.icon-button:focus-visible { outline: 1px solid var(--accent-hi); outline-offset: 2px; }
.icon-button:disabled { opacity: 0.4; cursor: default; }

.tag {
    position: absolute; top: 50%; padding: 4px 8px; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass-strong);
    font: 9px var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; color: var(--ink);
    white-space: nowrap; pointer-events: none; opacity: 0; transform: translateY(-50%); z-index: 2;
}
.right .tag { left: calc(100% + 8px); }
.left .tag { right: calc(100% + 8px); }
.icon-button:hover .tag, .icon-button:focus-visible .tag { opacity: 1; }
</style>
