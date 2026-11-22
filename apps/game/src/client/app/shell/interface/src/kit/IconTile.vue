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
/* the kit's glass (Button, IconButton): see-through, one hairline, frosted behind */
.tile {
    position: relative;
    display: grid;
    place-items: center;
    width: var(--size);
    height: var(--size);
    padding: 0;
    border: 1px solid var(--glass-edge);
    background: var(--glass);
    backdrop-filter: blur(8px);
    color: var(--ink-dim);
}

.tile:hover:not(:disabled) { border-color: var(--edge-lit); color: var(--ink); }
.tile.on { background: var(--glass-lit); border-color: var(--edge-lit-hi); color: #fff; }
.tile:focus-visible { outline: 1px solid var(--accent-hi); outline-offset: 2px; }
.tile:disabled { opacity: 0.4; cursor: default; }

.tag {
    position: absolute;
    top: 50%;
    padding: 4px 8px;
    border: 1px solid var(--glass-edge);
    background: var(--glass-strong);
    font: 9px var(--font-display);
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--ink);
    white-space: nowrap;
    pointer-events: none;
    opacity: 0;
    transform: translateY(-50%);
    z-index: 2;
}
.right .tag { left: calc(100% + 8px); }
.left .tag { right: calc(100% + 8px); }
.tile:hover .tag, .tile:focus-visible .tag { opacity: 1; }
</style>
