<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from "vue"
import { stars } from "./sky.ts"

/**
 * The night behind a full screen: flat dark and pixel stars, drawn once into a canvas at one
 * art pixel per `px` screen pixels, and again only when the screen changes size. Never
 * animated: the page re-uploads to the GPU on every repaint, so a moving sky is the engine's.
 */
const props = withDefaults(defineProps<{ seed?: string; px?: number }>(), { seed: "terraverse", px: 3 })
const canvas = ref<HTMLCanvasElement | null>(null)
let observer: ResizeObserver | null = null

function paint(): void {
    const el = canvas.value
    if (!el) return
    const width = Math.ceil(el.clientWidth / props.px)
    const height = Math.ceil(el.clientHeight / props.px)
    el.width = width
    el.height = height
    const g = el.getContext("2d")
    if (!g) throw new Error("Sky: no 2D context")
    for (const s of stars(props.seed, width, height)) {
        g.fillStyle = s.fill
        g.fillRect(s.x, s.y, s.size, s.size)
    }
}

onMounted(() => {
    observer = new ResizeObserver(paint)
    if (canvas.value) observer.observe(canvas.value)
})
onBeforeUnmount(() => observer?.disconnect())
</script>

<template>
    <div class="sky"><canvas ref="canvas" /><slot /></div>
</template>

<style scoped>
.sky { position: relative; background: var(--night-sky); overflow: hidden; }
canvas { position: absolute; inset: 0; width: 100%; height: 100%; image-rendering: pixelated; pointer-events: none; }
</style>
