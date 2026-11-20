<script setup lang="ts">
import { computed } from "vue"
import type { SlotRef, Stack } from "../../bridge/index.ts"
import type { Held } from "./hand.ts"
import Slot from "./Slot.vue"

/** A run of one inventory list, laid out `columns` wide: slots [start, end). */
const props = defineProps<{
    list: string
    stacks: (Stack | null)[]
    columns: number
    start?: number
    end?: number
    held: Held | null
}>()
const emit = defineEmits<{ press: [slot: SlotRef, stack: Stack | null, button: "left" | "right", shift: boolean, event: MouseEvent]; hover: [over: { slot: SlotRef; stack: Stack | null } | null] }>()

const range = computed(() => {
    const from = props.start ?? 0
    const to = props.end ?? props.stacks.length
    return Array.from({ length: to - from }, (_, i) => from + i)
})

function lifted(index: number, stack: Stack | null) {
    const h = props.held
    if (!h || h.from.list !== props.list || h.from.index !== index || !stack) return 0
    return h.count ?? stack.count
}
</script>

<template>
    <div class="grid" :style="{ gridTemplateColumns: `repeat(${columns}, auto)` }">
        <Slot v-for="i in range" :key="i" :stack="stacks[i] ?? null" :lifted="lifted(i, stacks[i] ?? null)"
            :data-list="list" :data-index="i"
            @press="(button, shift, event) => emit('press', { list: list, index: i }, stacks[i] ?? null, button, shift, event)"
            @hover="(inside) => emit('hover', inside ? { slot: { list: list, index: i }, stack: stacks[i] ?? null } : null)" />
    </div>
</template>

<style scoped>
.grid { display: grid; gap: calc(var(--u) * 1); justify-content: start; }
</style>
