<script setup lang="ts">
/**
 * A block you press: bevelled, hard-edged, sinks two pixels when held. Busy,
 * it's what was pressed and is under way: a spinner by its label, not pressable,
 * and not faded as a button that can't be.
 */
import Spinner from "./Spinner.vue"

withDefaults(defineProps<{ variant?: "primary" | "plain" | "danger"; size?: "lg" | "md" | "sm"; disabled?: boolean; busy?: boolean }>(), {
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
.btn {
    position: relative;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 10px;
    border: var(--b) solid var(--edge);
    background: var(--stone-3);
    box-shadow: var(--raised), var(--ledge);
    font-family: var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.06em;
    white-space: nowrap; /* a label is one line; the layout makes room for it */
    color: var(--ink);
    text-shadow: var(--b) var(--b) 0 rgb(0 0 0 / 0.5);
    transition: background 80ms, box-shadow 80ms, transform 60ms;
}

.lg { height: 56px; padding: 0 28px; font-size: 20px; }
.md { height: 44px; padding: 0 20px; font-size: 15px; }
.sm { height: 34px; padding: 0 14px; font-size: 12px; }

.btn:hover:not(:disabled) {
    background: var(--stone-4);
    outline: var(--b) solid var(--accent);
    outline-offset: 0;
}

.btn:active:not(:disabled) {
    transform: translateY(var(--b));
    box-shadow: var(--sunk);
}

.primary { background: var(--accent-lo); }
.primary:hover:not(:disabled) { background: var(--accent); outline-color: var(--accent-hi); }

.danger { background: var(--danger-lo); }
.danger:hover:not(:disabled) { background: var(--danger); outline-color: #ff8a73; }

.btn:disabled { opacity: 0.4; cursor: default; }
.btn.busy { opacity: 1; cursor: progress; }
</style>
