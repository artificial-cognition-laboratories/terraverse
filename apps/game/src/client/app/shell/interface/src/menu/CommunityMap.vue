<script setup lang="ts">
import { ref, watch } from "vue"
import { useBridge } from "../bridge/use.ts"
import { decodePreview, paint } from "../worldgen/fields.ts"

/** A world code's planet as a flat map (the preview's painting): a thumbnail. The globe is the native planet's. */
const props = defineProps<{ code: string }>()
const bridge = useBridge()
const canvas = ref<HTMLCanvasElement | null>(null)
const ready = ref(false)
const problem = ref<string | null>(null)
let generation = 0

watch([() => props.code, canvas], async ([code, el]) => {
    const current = ++generation
    ready.value = false
    problem.value = null
    if (!el) return
    try {
        const wire = await bridge.send("worldgen.preview", { code, date: 0.25, resolution: 128 })
        if (current !== generation) return
        const image = paint(decodePreview(wire), "terrain")
        el.width = image.width
        el.height = image.height
        const context = el.getContext("2d")
        if (!context) throw new Error("no 2d canvas for the world's map")
        context.putImageData(image, 0, 0)
        ready.value = true
    } catch (err) {
        if (current !== generation) return
        problem.value = err instanceof Error ? err.message : String(err)
    }
}, { immediate: true })
</script>

<template>
    <div class="preview">
        <canvas ref="canvas" :class="{ hidden: !ready }" />
        <p v-if="!ready">{{ problem ?? "Charting the planet…" }}</p>
    </div>
</template>

<style scoped>
.preview { position: relative; width: min(100%, 260px); aspect-ratio: 2; overflow: hidden; border: var(--b) solid var(--edge); background: #08121b; box-shadow: var(--sunk); }
canvas { width: 100%; height: 100%; image-rendering: pixelated; }
canvas.hidden { visibility: hidden; }
.preview p { position: absolute; inset: 0; display: grid; place-items: center; margin: 0; color: var(--ink-dim); font-size: 12px; }
</style>
