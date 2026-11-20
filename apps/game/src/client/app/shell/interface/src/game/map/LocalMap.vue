<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"

const props = defineProps<{
    image: ImageData
    pose: { u: number; v: number; heading: number }
    size: number
    round: boolean
    followHeading: boolean
    zoom: number
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const source = document.createElement("canvas")
const width = 256
const wrap = (value: number) => ((value % 1) + 1) % 1
const compass = computed(() => ["N", "E", "S", "W"].map((label, index) => {
    const angle = (index * 90 - (props.followHeading ? props.pose.heading : 0)) * Math.PI / 180
    return { label, style: { left: `${50 + 45 * Math.sin(angle)}%`, top: `${50 - 45 * Math.cos(angle)}%` } }
}))

function draw() {
    const context = canvas.value?.getContext("2d")
    if (!context) return
    // Keep the same ground area at every world size. The source is the
    // generated planet, so this stays a terrain map rather than a node radar.
    const zoom = Math.max(2, Math.min(128, props.size / 512 * 2 ** (props.zoom - 3)))
    const tile = width * zoom
    const left = -wrap(props.pose.u) * tile + width / 2
    const top = -props.pose.v * tile + width / 2
    context.fillStyle = "#0a1218"
    context.fillRect(0, 0, width, width)
    context.imageSmoothingEnabled = false
    context.save()
    context.translate(width / 2, width / 2)
    if (props.followHeading) context.rotate(-props.pose.heading * Math.PI / 180)
    context.translate(-width / 2, -width / 2)
    const reach = width * (Math.SQRT2 - 1) / 2
    const first = left + Math.floor((-reach - left) / tile) * tile
    for (let x = first; x < width + reach; x += tile)
        context.drawImage(source, x, top, tile, tile)
    context.restore()
}

function upload() {
    source.width = props.image.width
    source.height = props.image.height
    source.getContext("2d")!.putImageData(props.image, 0, 0)
    draw()
}

watch(() => props.image, upload)
watch(() => props.pose, draw, { deep: true })
watch(() => props.size, draw)
watch(() => [props.followHeading, props.zoom], draw)
onMounted(upload)
</script>

<template>
    <div class="local-map" :class="{ round }">
        <canvas ref="canvas" :width="width" :height="width" />
        <svg class="player" viewBox="0 0 26 30" :style="{ transform: `translate(-50%, -50%) rotate(${followHeading ? 0 : pose.heading}deg)` }" aria-label="Your direction">
            <path d="M13 2 24 27 13 21 2 27Z" />
        </svg>
        <span v-for="point in compass" :key="point.label" class="compass" :style="point.style">{{ point.label }}</span>
    </div>
</template>

<style scoped>
.local-map { position: relative; width: 100%; aspect-ratio: 1; overflow: hidden; background: #0a1218; }
.local-map.round { border-radius: 50%; }
canvas { display: block; width: 100%; height: 100%; image-rendering: pixelated; }
.player { position: absolute; top: 50%; left: 50%; width: 20px; height: 23px; overflow: visible; filter: drop-shadow(0 1px 0 #061015); pointer-events: none; }
.player path { fill: var(--accent-hi); stroke: #061015; stroke-width: 3; stroke-linejoin: miter; }
.compass { position: absolute; transform: translate(-50%, -50%); color: white; text-shadow: 1px 1px #071015, -1px -1px #071015; font: 11px var(--font-display); pointer-events: none; }
</style>
