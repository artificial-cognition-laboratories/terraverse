<script setup lang="ts">
import { computed } from "vue"
import type { Stack } from "../../bridge/index.ts"
import ItemSlot from "../ItemSlot.vue"
import "../cell.css"

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
    <button class="slot item-cell pickable" :class="{ result: result, lifted: !!lifted }" @mousedown.prevent="down" @contextmenu.prevent
        @mouseenter="emit('hover', true)" @mouseleave="emit('hover', false)">
        <ItemSlot :stack="shown" />
    </button>
</template>

<style scoped>
/* the hotbar's cell (cell.css); a craft's result a step bigger, its edge a step brighter */
.slot { cursor: default; }
.result { width: 3.25rem; height: 3.25rem; border-color: var(--glass-edge-hi); }
.lifted :deep(img) { opacity: 0.45; }
</style>
