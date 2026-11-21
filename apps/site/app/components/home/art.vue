<script setup lang="ts">
import type { CoverArt } from "~/lib/cover"

/**
 * Pixel art drawn crisp at any size: an image when there is one, otherwise the
 * scene given. `anchor` is where it stays put when the box crops it.
 */
const props = withDefaults(defineProps<{ art: CoverArt; image?: string | null; anchor?: "middle" | "bottom" }>(), {
    image: null,
    anchor: "middle",
})
const aspect = computed(() => (props.anchor === "bottom" ? "xMidYMax slice" : "xMidYMid slice"))
</script>

<template>
    <div class="art" :style="{ background: art.sky }">
        <img v-if="image" :src="image" alt="" loading="lazy" draggable="false" />
        <svg v-else :viewBox="`0 0 ${art.width} ${art.height}`" :preserveAspectRatio="aspect" shape-rendering="crispEdges" aria-hidden="true">
            <rect v-for="(p, i) in art.pixels" :key="i" :x="p.x" :y="p.y" width="1" :height="p.h" :fill="p.fill" />
        </svg>
    </div>
</template>

<style scoped lang="scss">
.art {
    position: relative;
    overflow: hidden;
}

img,
svg {
    display: block;
    width: 100%;
    height: 100%;
    object-fit: cover;
    image-rendering: pixelated;
}
</style>
