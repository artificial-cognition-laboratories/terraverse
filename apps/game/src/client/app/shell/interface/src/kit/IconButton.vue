<script setup lang="ts">
/**
 * The game's square icon button, as the pause menu's profile button is
 * drawn: a raised stone square, lit and ringed on hover or when on. Its
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
    position: relative; display: grid; place-items: center; width: 48px; height: 48px; padding: 0;
    color: var(--ink); background: var(--stone-2); border: var(--b) solid var(--edge);
    box-shadow: var(--raised), var(--ledge);
}
.icon-button:hover:not(:disabled), .icon-button.on { background: var(--stone-3); outline: var(--b) solid var(--accent); }
.icon-button:focus-visible { outline: var(--b) solid var(--accent-hi); }
.icon-button:disabled { opacity: 0.4; cursor: default; }

.tag {
    position: absolute; top: 50%; padding: 4px 8px; border: var(--b) solid var(--edge); background: var(--stone-0);
    box-shadow: var(--raised); font: 10px var(--font-display); text-transform: uppercase; color: var(--ink);
    white-space: nowrap; pointer-events: none; opacity: 0; transform: translateY(-50%);
    transition: opacity 80ms steps(2); z-index: 2;
}
.right .tag { left: calc(100% + 8px); }
.left .tag { right: calc(100% + 8px); }
.icon-button:hover .tag, .icon-button:focus-visible .tag { opacity: 1; }
</style>
