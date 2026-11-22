<script setup lang="ts">
/**
 * A block you press: a pane of glass with a hairline edge, hard corners; primary is the
 * glass tinted with the deep ocean. Held, it sinks a pixel. Busy,
 * it's what was pressed and is under way: a spinner by its label, not pressable,
 * and not faded as a button that can't be.
 */
import Spinner from "./Spinner.vue"

withDefaults(defineProps<{ variant?: "primary" | "plain" | "ghost" | "danger"; size?: "lg" | "md" | "sm"; disabled?: boolean; busy?: boolean }>(), {
    variant: "plain",
    size: "md",
    disabled: false,
    busy: false,
})
</script>

<template>
    <button class="btn" :class="[variant, size, { busy }]" :disabled="disabled || busy" :aria-busy="busy">
        <Spinner v-if="busy" />
        <slot />
    </button>
</template>

<style scoped>
/* The website's glass (apps/site home/download-tiles.vue): the night lifted and see-through, one
   hairline edge, frosted behind (the page by backdrop-filter, the engine's frame by the shell).
   Primary is that glass tinted with the deep ocean, its edge lit, as the site lights a visitor's
   own download. */
.btn {
    position: relative;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 10px;
    border: 1px solid var(--glass-edge);
    background: var(--glass);
    backdrop-filter: blur(8px);
    font-family: var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.06em;
    white-space: nowrap; /* a label is one line; the layout makes room for it */
    color: var(--ink);
}

.lg { height: 3.1rem; padding: 0 1.6rem; font-size: 0.85rem; }
.md { height: 2.6rem; padding: 0 1.2rem; font-size: 0.75rem; }
.sm { height: 2.1rem; padding: 0 0.8rem; font-size: 0.65rem; }

/* hover is quiet and instant: the edge lit faintly, the label a touch brighter; the glass stays */
.btn:hover:not(:disabled) { border-color: var(--edge-lit); color: #fff; }
.btn:focus-visible { outline: 1px solid var(--accent-hi); outline-offset: 2px; }
.btn:active:not(:disabled) { transform: translateY(1px); }

.primary { background-color: var(--glass-lit); border-color: var(--edge-lit-hi); color: #fff; }
.primary:hover:not(:disabled) { border-color: rgb(99 163 233 / 0.75); }

.ghost { background: transparent; border-color: transparent; backdrop-filter: none; color: var(--ink-dim); }
.ghost:hover:not(:disabled) { border-color: var(--glass-edge); color: var(--ink); }

.danger { color: var(--danger); border-color: rgb(248 81 73 / 0.4); }
.danger:hover:not(:disabled) { background: rgb(110 31 27 / 0.6); border-color: var(--danger); color: #fff; }

.btn:disabled { opacity: 0.4; cursor: default; }
.btn.busy { opacity: 1; cursor: progress; }
</style>
