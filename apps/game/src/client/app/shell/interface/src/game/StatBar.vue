<script setup lang="ts">
import { computed } from "vue"
import PixelIcon from "./PixelIcon.vue"
import { BUBBLE, HEART, PALETTES, leftHalf } from "./icons.ts"

/** A row of icons, each worth two points, Minecraft-style (hearts, bubbles). */
const props = defineProps<{ kind: "heart" | "bubble"; value: number; max: number; reverse?: boolean }>()

const rows = computed(() => (props.kind === "heart" ? HEART : BUBBLE))
const palette = computed(() => PALETTES[props.kind])
const icons = computed(() =>
    Array.from({ length: Math.ceil(props.max / 2) }, (_, i) => {
        const points = props.value - i * 2
        return points >= 2 ? "full" : points === 1 ? "half" : "empty"
    }),
)
</script>

<template>
    <div class="bar" :class="{ reverse: reverse }">
        <span v-for="(state, i) in icons" :key="i" class="icon">
            <PixelIcon v-if="kind === 'heart' || state !== 'empty'" :rows="rows" :palette="palette.empty" />
            <PixelIcon v-if="state === 'full'" class="over" :rows="rows" :palette="palette.full" />
            <PixelIcon v-else-if="state === 'half'" class="over" :rows="leftHalf(rows)" :palette="palette.full" />
        </span>
    </div>
</template>

<style scoped>
.bar { display: flex; } /* 9-wide icons on an 8 pitch overlap by one pixel, as in Minecraft */
.bar.reverse { flex-direction: row-reverse; }
.icon { position: relative; display: block; width: calc(var(--u) * 8); height: calc(var(--u) * 8); }
.icon svg { position: absolute; left: 0; top: 0; }
</style>
