<script setup lang="ts">
import type { Stack } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import ItemSlot from "./ItemSlot.vue"
import "./cell.css"

/** The hotbar; with the mouse free (an object held, or paused) a slot's picked by clicking it: the build's palette. */
const props = defineProps<{ stacks: (Stack | null)[]; selected: number; free: boolean }>()
const bridge = useBridge()
const pick = (slot: number) => {
    if (props.free) void bridge.send("hotbar.select", { slot: slot })
}
</script>

<template>
    <div class="hotbar item-strip" :class="{ free: free }">
        <div v-for="(stack, i) in stacks" :key="i" class="item-cell" :class="{ selected: i === selected, pickable: free }" @click="pick(i)">
            <ItemSlot :stack="stack" />
        </div>
    </div>
</template>
<style scoped>
/* the cells and their strip are every item cell's (cell.css) */
.hotbar.free { pointer-events: auto; }
</style>
