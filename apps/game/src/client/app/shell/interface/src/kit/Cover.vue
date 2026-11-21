<script setup lang="ts">
import { computed } from "vue"
import { cover } from "./cover.ts"

/**
 * A card's picture: the image when there is one, otherwise a pixel landscape drawn
 * from the seed, so a listing without art still looks like a place, and always the same one.
 */
const props = withDefaults(defineProps<{ seed: string; image?: string | null; width?: number }>(), { image: null, width: 32 })
const art = computed(() => cover(props.seed, props.width))
</script>

<template>
    <div class="cover">
        <img v-if="image" :src="image" alt="" draggable="false">
        <svg v-else :viewBox="`0 0 ${art.width} ${art.height}`" preserveAspectRatio="xMidYMid slice" shape-rendering="crispEdges">
            <rect :width="art.width" :height="art.height" :fill="art.sky" />
            <rect v-for="(p, i) in art.pixels" :key="i" :x="p.x" :y="p.y" width="1" :height="p.h" :fill="p.fill" />
        </svg>
    </div>
</template>

<style scoped>
.cover { position: relative; overflow: hidden; background: var(--stone-0); }
img, svg { display: block; width: 100%; height: 100%; object-fit: cover; image-rendering: pixelated; }
</style>
