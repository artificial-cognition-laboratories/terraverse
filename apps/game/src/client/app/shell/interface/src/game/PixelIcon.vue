<script setup lang="ts">
import { computed } from "vue"
import { drawn } from "./pixelPaths.ts"

/**
 * Pixel art from a character map: each row is a string, each character a
 * palette key ('.' is transparent). Renders crisp at any --u scale.
 */
const props = defineProps<{ rows: readonly string[]; palette: Record<string, string> }>()

const art = computed(() => drawn(props.rows, props.palette))
</script>

<template>
    <svg :viewBox="`0 0 ${art.width} ${rows.length}`" :width="`calc(var(--u) * ${art.width})`"
        :height="`calc(var(--u) * ${rows.length})`" shape-rendering="crispEdges">
        <path v-for="p in art.paths" :key="p.fill" :d="p.d" :fill="p.fill" />
    </svg>
</template>
