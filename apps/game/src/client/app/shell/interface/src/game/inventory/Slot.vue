<script setup lang="ts">
import { computed } from "vue"
import type { Stack } from "../../bridge/index.ts"
import ItemSlot from "../ItemSlot.vue"

/**
 * One inventory cell you can click. `lifted` is how many of this stack the
 * cursor is holding: the slot shows what's left behind.
 */
const props = defineProps<{ stack: Stack | null; lifted?: number; result?: boolean }>()
const emit = defineEmits<{ press: [button: "left" | "right", shift: boolean, event: MouseEvent]; hover: [inside: boolean] }>()

const shown = computed(() => {
    if (!props.stack || !props.lifted) return props.stack
    const left = props.stack.count - props.lifted
    return left > 0 ? { ...props.stack, count: left } : null
})

function down(e: MouseEvent) {
    if (e.button !== 0 && e.button !== 2) return
	emit("press", e.button === 2 ? "right" : "left", e.shiftKey, e)
}
</script>

<template>
    <button class="slot" :class="{ result: result, lifted: !!lifted }" @mousedown.prevent="down" @contextmenu.prevent
        @mouseenter="emit('hover', true)" @mouseleave="emit('hover', false)">
        <ItemSlot :stack="shown" />
    </button>
</template>

<style scoped>
.slot {
    position: relative;
    display: grid;
    place-items: center;
    width: calc(var(--u) * 18);
    height: calc(var(--u) * 18);
    padding: 0;
    border: var(--b) solid var(--edge);
    background: var(--stone-0);
    box-shadow: var(--sunk);
    cursor: default;
}

.slot:hover { background: #1b2730; box-shadow: inset 0 0 0 var(--b) var(--accent-lo), var(--sunk); }
.result { width: calc(var(--u) * 24); height: calc(var(--u) * 24); }
.lifted :deep(img) { opacity: 0.45; }
</style>
