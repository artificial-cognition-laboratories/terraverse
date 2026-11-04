<script setup lang="ts">
import { computed } from "vue"
import type { Stack } from "../bridge"
import { useStore } from "./store"

/** One stack as Minecraft draws it: icon, count, and a durability bar once worn. */
const props = defineProps<{ stack: Stack | null }>()
const store = useStore()

const info = computed(() => (props.stack ? store.items[props.stack.item] ?? null : null))
// Luanti wear runs 0 (new) to 65535 (broken).
const durability = computed(() => (props.stack && props.stack.wear > 0 ? 1 - props.stack.wear / 65536 : null))
</script>

<template>
    <div class="slot">
        <template v-if="stack">
            <img v-if="info?.icon" :src="info.icon" :alt="info.description" draggable="false">
            <span v-else class="missing">{{ (info?.description ?? stack.item).slice(0, 2) }}</span>
            <span v-if="stack.count > 1" class="count">{{ stack.count }}</span>
            <span v-if="durability !== null" class="wear">
                <span :style="{ width: `${durability * 100}%`, background: `hsl(${durability * 120} 90% 50%)` }" />
            </span>
        </template>
    </div>
</template>

<style scoped>
.slot {
    position: relative;
    width: calc(var(--u) * 16);
    height: calc(var(--u) * 16);
}

img {
    width: 100%;
    height: 100%;
    image-rendering: pixelated;
}

.missing {
    display: grid;
    place-items: center;
    height: 100%;
    font-size: calc(var(--u) * 6);
    color: #fff;
    background: rgb(255 0 255 / 0.4);
}

.count {
    position: absolute;
    right: 0;
    bottom: calc(var(--u) * -1);
    font: 400 calc(var(--u) * 6) / 1 var(--font-pixel);
    letter-spacing: -0.08em;
    color: var(--ink);
    text-shadow: var(--b) var(--b) 0 var(--edge);
}

.wear {
    position: absolute;
    left: calc(var(--u) * 2);
    right: calc(var(--u) * 2);
    bottom: calc(var(--u) * 1);
    height: calc(var(--u) * 2);
    background: #000;
}

.wear span { display: block; height: 50%; }
</style>
