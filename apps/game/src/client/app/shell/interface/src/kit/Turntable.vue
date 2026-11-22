<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from "vue"

/**
 * A block or an item, turned in the hand: drag to rotate it, double-click to set it back.
 * A block is a cube of its faces; an item is its icon extruded one texel thick, as the game
 * draws it held. Pixels stay square, and it's lit by the one light, top-left. Still unless
 * it's dragged: nothing animates on its own.
 */
const props = withDefaults(defineProps<{
    /** A block's faces; when set, it's drawn as a cube. */
    faces?: { top: string; bottom: string; side: string } | null
    /** An item's icon, drawn when there are no faces. */
    icon?: string | null
    /** Edge of the cube, or width of the item, in CSS pixels. Unset, it fills its parent and fits the solid to it. */
    size?: number | null
}>(), { faces: null, icon: null, size: null })

// filling: the solid's edge from the box it's given, room left for its corners as it turns
const box = ref<HTMLElement | null>(null)
const fitted = ref(0)
let watching: ResizeObserver | null = null
onMounted(() => {
    if (props.size !== null || !box.value) return
    watching = new ResizeObserver(([entry]) => {
        const r = entry!.contentRect
        fitted.value = Math.floor(Math.min(r.width, r.height) / 2.1)
    })
    watching.observe(box.value)
})
onBeforeUnmount(() => watching?.disconnect())
const edge = computed(() => (props.size ?? fitted.value) * zoom.value)

/** How close: 1 fits the box; the wheel changes it, and so can whoever holds the model. */
const zoom = defineModel<number>("zoom", { default: 1 })
const ZOOM = { min: 0.4, max: 8 }

const REST = { yaw: -45, pitch: -30 }
const yaw = ref(REST.yaw)
const pitch = ref(REST.pitch)
let dragging: { x: number; y: number } | null = null

function down(e: PointerEvent) {
    dragging = { x: e.clientX, y: e.clientY }
    ;(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId)
}
function move(e: PointerEvent) {
    if (!dragging) return
    yaw.value += (e.clientX - dragging.x) * 0.6
    pitch.value = Math.max(-89, Math.min(89, pitch.value - (e.clientY - dragging.y) * 0.6))
    dragging = { x: e.clientX, y: e.clientY }
}
function up() {
    dragging = null
}
function wheel(e: WheelEvent) {
    zoom.value = Math.min(ZOOM.max, Math.max(ZOOM.min, zoom.value * Math.exp(-e.deltaY * 0.0015)))
}
function reset() {
    yaw.value = REST.yaw
    pitch.value = REST.pitch
}

// the light, in the viewer's space (CSS: y down, z toward you): above, left and in front
const LIGHT = normal([-0.45, -0.8, 0.55])
type Vec = readonly [number, number, number]

/** How lit a face with this normal (in the solid's own space) is, as it's turned now. */
function light(n: Vec): number {
    const y = (yaw.value * Math.PI) / 180, p = (pitch.value * Math.PI) / 180
    // rotateY(yaw), then rotateX(pitch): as the transform below applies them
    const x1 = n[0] * Math.cos(y) + n[2] * Math.sin(y)
    const z1 = -n[0] * Math.sin(y) + n[2] * Math.cos(y)
    const y2 = n[1] * Math.cos(p) - z1 * Math.sin(p)
    const z2 = n[1] * Math.sin(p) + z1 * Math.cos(p)
    const d = x1 * LIGHT[0] + y2 * LIGHT[1] + z2 * LIGHT[2]
    return 0.5 + 0.5 * Math.max(0, d)
}

const transform = computed(() => `rotateX(${pitch.value}deg) rotateY(${yaw.value}deg)`)
const half = computed(() => edge.value / 2)

const CUBE: { id: string; face: "top" | "bottom" | "side"; n: Vec; place: (h: number) => string }[] = [
    { id: "front", face: "side", n: [0, 0, 1], place: h => `translateZ(${h}px)` },
    { id: "back", face: "side", n: [0, 0, -1], place: h => `rotateY(180deg) translateZ(${h}px)` },
    { id: "right", face: "side", n: [1, 0, 0], place: h => `rotateY(90deg) translateZ(${h}px)` },
    { id: "left", face: "side", n: [-1, 0, 0], place: h => `rotateY(-90deg) translateZ(${h}px)` },
    { id: "top", face: "top", n: [0, -1, 0], place: h => `rotateX(90deg) translateZ(${h}px)` },
    { id: "bottom", face: "bottom", n: [0, 1, 0], place: h => `rotateX(-90deg) translateZ(${h}px)` },
]
const cube = computed(() => props.faces ? CUBE.map(f => ({
    id: f.id,
    style: {
        backgroundImage: `url("${props.faces![f.face]}")`,
        transform: f.place(half.value),
        filter: `brightness(${light(f.n).toFixed(3)})`,
    },
})) : [])

// an item: its icon in thin slices through one texel of depth, the outer two lit as faces
const SLICES = 12
const slab = computed(() => {
    if (props.faces || !props.icon) return []
    const depth = edge.value / 16
    const front = light([0, 0, 1]), back = light([0, 0, -1])
    return Array.from({ length: SLICES }, (_, i) => {
        const z = -depth / 2 + (depth * i) / (SLICES - 1)
        const shade = i === SLICES - 1 ? front : i === 0 ? back : 0.45
        return { id: i, style: { backgroundImage: `url("${props.icon}")`, transform: `translateZ(${z}px)`, filter: `brightness(${shade.toFixed(3)})` } }
    })
})

function normal(v: Vec): Vec {
    const l = Math.hypot(v[0], v[1], v[2])
    return [v[0] / l, v[1] / l, v[2] / l]
}
</script>

<template>
    <div ref="box" class="turntable" :class="{ fill: size === null }" :style="size === null ? undefined : { width: `${size * 1.8}px`, height: `${size * 1.8}px` }"
        @pointerdown="down" @pointermove="move" @pointerup="up" @pointercancel="up" @dblclick="reset" @wheel.prevent="wheel">
        <div class="solid" :style="{ width: `${edge}px`, height: `${edge}px`, transform: transform }">
            <div v-for="f in cube" :key="f.id" class="face" :style="f.style" />
            <div v-for="s in slab" :key="s.id" class="face" :style="s.style" />
        </div>
        <p v-if="!faces && !icon" class="nothing">nothing to draw</p>
    </div>
</template>

<style scoped>
.turntable { position: relative; display: grid; place-items: center; overflow: hidden; perspective: 2400px; cursor: grab; user-select: none; touch-action: none; }
.turntable:active { cursor: grabbing; }
.fill { width: 100%; height: 100%; min-width: 0; min-height: 0; }
.solid { position: relative; transform-style: preserve-3d; }
.face { position: absolute; inset: 0; background-size: 100% 100%; background-repeat: no-repeat; image-rendering: pixelated; backface-visibility: visible; }
.nothing { position: absolute; margin: 0; color: var(--ink-faint); font-size: 12px; }
</style>
