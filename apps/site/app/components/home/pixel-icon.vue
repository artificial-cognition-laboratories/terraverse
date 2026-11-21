<script setup lang="ts">
import type { PixelIcon } from "~/config/icons"

/** A 9×9 icon from rows of letters, each letter a colour from its palette. */
const props = withDefaults(defineProps<{ icon: PixelIcon; size?: number }>(), { size: 27 })
const cells = computed(() =>
    props.icon.rows.flatMap((row, y) =>
        [...row].flatMap((letter, x) => {
            const fill = props.icon.palette[letter]
            return fill ? [{ x: x, y: y, fill: fill }] : []
        }),
    ),
)
</script>

<template>
    <svg :width="size" :height="size" viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
        <rect v-for="c in cells" :key="`${c.x},${c.y}`" :x="c.x" :y="c.y" width="1" height="1" :fill="c.fill" />
    </svg>
</template>
