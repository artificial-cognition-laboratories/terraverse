<script setup lang="ts">
import { computed } from "vue"

/**
 * Pixel art from a character map: each row is a string, each character a
 * palette key ('.' is transparent). Renders crisp at any --u scale.
 */
const props = defineProps<{ rows: readonly string[]; palette: Record<string, string> }>()

const width = computed(() => Math.max(...props.rows.map((r) => r.length)))
const pixels = computed(() =>
    props.rows.flatMap((row, y) =>
        [...row].flatMap((key, x) => {
            const fill = props.palette[key]
            return key === "." || !fill ? [] : [{ x: x, y: y, fill: fill }]
        }),
    ),
)
</script>

<template>
    <svg :viewBox="`0 0 ${width} ${rows.length}`" :width="`calc(var(--u) * ${width})`"
        :height="`calc(var(--u) * ${rows.length})`" shape-rendering="crispEdges">
        <rect v-for="p in pixels" :key="`${p.x},${p.y}`" :x="p.x" :y="p.y" width="1" height="1" :fill="p.fill" />
    </svg>
</template>
