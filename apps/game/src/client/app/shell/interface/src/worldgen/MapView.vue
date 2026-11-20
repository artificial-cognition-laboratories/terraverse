<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"

/**
 * The world, flat. Drag to pan — endlessly east–west (the world wraps), up to
 * the polar edges north–south. Scroll to zoom, click to set the spawn point.
 */
const props = withDefaults(defineProps<{ image: ImageData; size: number; allowSpawn?: boolean; showCoords?: boolean }>(),
    { allowSpawn: true, showCoords: true })
const spawn = defineModel<{ x: number; z: number } | null>("spawn", { required: true })

const canvas = ref<HTMLCanvasElement | null>(null)
const source = document.createElement("canvas")
const view = ref({ u: 0, v: 0, zoom: 1 }) // top-left of the view in world fractions
const hover = ref<{ x: number; z: number } | null>(null)
let drag: { x: number; y: number; moved: boolean } | null = null

const wrap = (n: number) => ((n % 1) + 1) % 1
// north–south the view stops at the edges: its top lies within [0, 1 - 1/zoom]
const clampV = () => (view.value.v = Math.max(0, Math.min(1 - 1 / view.value.zoom, view.value.v)))

function draw() {
    const el = canvas.value
    if (!el) return
    const ctx = el.getContext("2d")!
    const w = el.width, h = el.height
    const tile = w * view.value.zoom
    ctx.imageSmoothingEnabled = false
    ctx.clearRect(0, 0, w, h)
    const ox = -wrap(view.value.u) * tile, oy = -view.value.v * tile
    for (let tx = ox; tx < w; tx += tile) ctx.drawImage(source, tx, oy, tile, tile)

    if (spawn.value) {
        const sx = wrap(spawn.value.x / props.size - view.value.u) * tile
        const sy = (spawn.value.z / props.size - view.value.v) * tile
        for (const [dx, dy] of [[0, 0], [-tile, 0]] as const) {
            const px = Math.round(sx + dx), py = Math.round(sy + dy)
            if (px < -8 || py < -8 || px > w + 8 || py > h + 8) continue
            ctx.fillStyle = "#07080a"
            ctx.fillRect(px - 7, py - 2, 14, 4)
            ctx.fillRect(px - 2, py - 7, 4, 14)
            ctx.fillStyle = "rgb(64 184 236)"
            ctx.fillRect(px - 6, py - 1, 12, 2)
            ctx.fillRect(px - 1, py - 6, 2, 12)
        }
    }
}

function toWorld(e: MouseEvent) {
    const el = canvas.value!
    const rect = el.getBoundingClientRect()
    const tile = rect.width * view.value.zoom
    const u = wrap(view.value.u + (e.clientX - rect.left) / tile)
    const v = Math.max(0, Math.min(0.9999, view.value.v + (e.clientY - rect.top) / tile))
    return { x: Math.floor(u * props.size), z: Math.floor(v * props.size) }
}

function onDown(e: MouseEvent) {
    drag = { x: e.clientX, y: e.clientY, moved: false }
}

function onMove(e: MouseEvent) {
    hover.value = toWorld(e)
    if (!drag) return
    const tile = canvas.value!.getBoundingClientRect().width * view.value.zoom
    if (Math.abs(e.clientX - drag.x) + Math.abs(e.clientY - drag.y) > 3) drag.moved = true
    view.value.u -= (e.clientX - drag.x) / tile
    view.value.v -= (e.clientY - drag.y) / tile
    clampV()
    drag.x = e.clientX
    drag.y = e.clientY
    draw()
}

function onUp(e: MouseEvent) {
    if (drag && !drag.moved && props.allowSpawn) spawn.value = toWorld(e)
    drag = null
}

function onLeave() {
    drag = null
    hover.value = null
}

function onWheel(e: WheelEvent) {
    const rect = canvas.value!.getBoundingClientRect()
    const before = { u: (e.clientX - rect.left) / (rect.width * view.value.zoom), v: (e.clientY - rect.top) / (rect.height * view.value.zoom) }
    view.value.zoom = Math.min(8, Math.max(1, view.value.zoom * (e.deltaY < 0 ? 1.25 : 0.8)))
    const after = { u: (e.clientX - rect.left) / (rect.width * view.value.zoom), v: (e.clientY - rect.top) / (rect.height * view.value.zoom) }
    view.value.u += before.u - after.u
    view.value.v += before.v - after.v
    clampV()
    draw()
}

function upload() {
    source.width = props.image.width
    source.height = props.image.height
    source.getContext("2d")!.putImageData(props.image, 0, 0)
    draw()
}

const readout = computed(() => (hover.value ? `${hover.value.x}, ${hover.value.z}` : "—"))

watch(() => props.image, upload)
watch(spawn, draw)
onMounted(upload)
</script>

<template>
    <div class="map">
        <canvas ref="canvas" width="512" height="512" @mousedown="onDown" @mousemove="onMove" @mouseup="onUp"
            @mouseleave="onLeave" @wheel.prevent="onWheel" />
        <div v-if="showCoords" class="hud">
            <span>{{ readout }}</span>
            <span>×{{ view.zoom.toFixed(1) }}</span>
        </div>
    </div>
</template>

<style scoped>
.map { position: relative; border: var(--b) solid var(--edge); background: var(--stone-0); }
canvas { display: block; width: 100%; height: 100%; image-rendering: pixelated; cursor: crosshair; }
.hud {
    position: absolute;
    left: 0;
    right: 0;
    bottom: 0;
    display: flex;
    justify-content: space-between;
    padding: 4px 8px;
    font: 11px var(--font-display);
    background: rgb(7 8 10 / 0.7);
    pointer-events: none;
}
</style>
