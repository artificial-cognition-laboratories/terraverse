<script setup lang="ts">
/**
 * A small square button that shows a picture, not words. The label appears as
 * a tag beside it on hover (`side` says which side), so rows of tiles stay compact.
 */
withDefaults(defineProps<{ label: string; on?: boolean; disabled?: boolean; side?: "left" | "right"; size?: number }>(), {
    on: false,
    disabled: false,
    side: "right",
    size: 44,
})
</script>

<template>
    <button class="tile" :class="[side, { on: on }]" :disabled="disabled" :aria-label="label" :aria-pressed="on"
        :style="{ '--size': `${size}px` }">
        <slot />
        <span class="tag">{{ label }}</span>
    </button>
</template>

<style scoped>
.tile {
    position: relative;
    display: grid;
    place-items: center;
    width: var(--size);
    height: var(--size);
    padding: 0;
    border: var(--b) solid var(--edge);
    background: var(--stone-1);
    box-shadow: var(--raised);
}

.tile:hover:not(:disabled) { background: var(--stone-3); }
.tile.on { background: var(--stone-3); box-shadow: inset 0 0 0 var(--b) var(--accent), var(--raised); }
.tile:disabled { opacity: 0.4; cursor: default; }

.tag {
    position: absolute;
    top: 50%;
    padding: 4px 8px;
    border: var(--b) solid var(--edge);
    background: var(--stone-0);
    box-shadow: var(--raised);
    font: 10px var(--font-display);
    text-transform: uppercase;
    color: var(--ink);
    white-space: nowrap;
    pointer-events: none;
    opacity: 0;
    transform: translateY(-50%);
    transition: opacity 80ms steps(2);
    z-index: 2;
}
.right .tag { left: calc(100% + 8px); }
.left .tag { right: calc(100% + 8px); }
.tile:hover .tag, .tile:focus-visible .tag { opacity: 1; }
</style>
