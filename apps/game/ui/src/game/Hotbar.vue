<script setup lang="ts">
import type { Stack } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import ItemSlot from "./ItemSlot.vue"

/** The hotbar; with the mouse free (an object held, or paused) a slot's picked by clicking it: the build's palette. */
const props = defineProps<{ stacks: (Stack | null)[]; selected: number; free: boolean }>()
const bridge = useBridge()
const pick = (slot: number) => {
    if (props.free) void bridge.send("hotbar.select", { slot: slot })
}
</script>

<template>
    <div class="hotbar" :class="{ free: free }">
        <div v-for="(stack, i) in stacks" :key="i" class="cell" :class="{ selected: i === selected }" @click="pick(i)">
            <ItemSlot :stack="stack" />
        </div>
    </div>
</template>
<style scoped>
.hotbar {
    display: flex;
    gap: var(--u);
    padding: var(--u);
    border: var(--b) solid var(--edge);
    background: rgb(21 24 27 / 0.94);
    box-shadow: var(--raised), var(--ledge);
}

.hotbar.free { pointer-events: auto; }
.hotbar.free .cell { cursor: pointer; }

.cell {
    position: relative;
    display: grid;
    place-items: center;
    width: calc(var(--u) * 20);
    height: calc(var(--u) * 20);
    border: var(--b) solid var(--edge);
    background: var(--stone-0);
    box-shadow: var(--sunk);
}

.cell.selected {
    background: var(--stone-2);
    outline: var(--b) solid var(--accent-hi);
    box-shadow: inset 0 calc(var(--u) * -1) 0 var(--accent), var(--sunk);
    z-index: 1;
}
</style>
