<script setup lang="ts">
import { onMounted, ref, watch } from "vue"

/**
 * A body in miniature, for the system tiles: the star as a disc in its colour,
 * the planet as a tiny lit globe drawn from the current preview, a moon as
 * cratered grey. 32px; drawn once per change, not animated.
 */
const props = defineProps<{ kind: "star" | "planet" | "moon"; color?: readonly [number, number, number]; image?: ImageData | null }>()

const SIZE = 32
const canvas = ref<HTMLCanvasElement | null>(null)

function drawPlanet() {
    const ctx = canvas.value?.getContext("2d")
    const src = props.image
    if (!ctx || !src) return
    const out = ctx.createImageData(SIZE, SIZE)
    const r = SIZE / 2 - 1
    for (let py = 0; py < SIZE; py++)
        for (let px = 0; px < SIZE; px++) {
            const x = (px + 0.5 - SIZE / 2) / r, y = (py + 0.5 - SIZE / 2) / r
            const d = x * x + y * y
            if (d > 1) continue
            const z = Math.sqrt(1 - d)
            const u = ((Math.atan2(x, z) / (2 * Math.PI)) % 1 + 1) % 1
            const v = Math.asin(y) / Math.PI + 0.5
            const si = (Math.floor(v * (src.height - 1)) * src.width + Math.floor(u * (src.width - 1))) * 4
            const light = 0.25 + 0.75 * Math.max(0, -x * 0.55 - y * 0.25 + z * 0.8)
            const o = (py * SIZE + px) * 4
            out.data[o] = src.data[si]! * light
            out.data[o + 1] = src.data[si + 1]! * light
            out.data[o + 2] = src.data[si + 2]! * light
            out.data[o + 3] = 255
        }
    ctx.putImageData(out, 0, 0)
}

// after the render: a moon's sketch turning into a picture has only just got its canvas
watch(() => [props.image, props.kind], drawPlanet, { flush: "post" })
onMounted(drawPlanet)
</script>

<template>
    <canvas v-if="kind === 'planet'" ref="canvas" class="icon" :width="SIZE" :height="SIZE" />
    <span v-else-if="kind === 'star'" class="icon star" :style="{ '--c': (color ?? [255, 244, 220]).join(' ') }" />
    <span v-else class="icon moon" />
</template>

<style scoped>
.icon { display: block; width: 32px; height: 32px; image-rendering: pixelated; }
.star {
    width: 24px;
    height: 24px;
    border-radius: 50%;
    background: radial-gradient(circle at 40% 38%, #fff, rgb(var(--c)) 45%, rgb(var(--c) / 0.8));
    box-shadow: 0 0 10px 2px rgb(var(--c) / 0.6);
}
.moon {
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background:
        radial-gradient(circle at 30% 35%, #8a8a8a 0 2px, transparent 3px),
        radial-gradient(circle at 65% 60%, #858585 0 3px, transparent 4px),
        radial-gradient(circle at 55% 25%, #8d8d8d 0 1px, transparent 2px),
        radial-gradient(circle at 40% 40%, #cfcfcf, #9a9a9a 70%);
}
</style>
