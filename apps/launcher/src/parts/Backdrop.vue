<script setup lang="ts">
import { onMounted, ref } from "vue"

/**
 * A planet rising at the bottom right, drawn once at low resolution and scaled
 * up pixelated: the launcher's stand-in for the planetarium's home shot.
 * Static, so the page never repaints for it.
 */
const W = 320, H = 180
const canvas = ref<HTMLCanvasElement | null>(null)

// a small, seeded value noise: the same planet every time
function hash(x: number, y: number) {
    const h = Math.sin(x * 127.1 + y * 311.7) * 43758.5453
    return h - Math.floor(h)
}
function noise(x: number, y: number) {
    const xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi
    const u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf)
    const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1)
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v
}
const fbm = (x: number, y: number) => noise(x, y) * 0.55 + noise(x * 2.1, y * 2.1) * 0.3 + noise(x * 4.3, y * 4.3) * 0.15

const SEA = [14, 58, 96], SHELF = [22, 92, 132], LAND = [70, 120, 62], HIGH = [128, 118, 92], ICE = [222, 236, 244]
const ACCENT = [0, 148, 210]
const TILT_SIN = Math.sin(0.45), TILT_COS = Math.cos(0.45)

onMounted(() => {
    const context = canvas.value?.getContext("2d")
    if (!context) throw new Error("backdrop: no 2d context")
    const image = context.createImageData(W, H)
    const cx = 236, cy = 262, r = 190
    // light from the top left, as every surface in the kit
    const light = [-0.55, -0.62, 0.56]
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
        const i = (y * W + x) * 4
        const dx = (x - cx) / r, dy = (y - cy) / r, d2 = dx * dx + dy * dy
        let rgb: number[]
        if (d2 <= 1) {
            const nz = Math.sqrt(1 - d2)
            const lit = Math.max(0, dx * light[0]! + dy * light[1]! + nz * light[2]!)
            // tip the pole away from us, then map to longitude / latitude so the land wraps round it
            const py = dy * TILT_COS + nz * TILT_SIN, pz = nz * TILT_COS - dy * TILT_SIN
            const lon = Math.atan2(dx, pz), lat = Math.asin(Math.max(-1, Math.min(1, py)))
            const h = fbm(lon * 3.2 + 3.7, lat * 3.2 + 1.3)
            const pole = Math.abs(lat) > 1.25 - fbm(lon * 3, 9) * 0.12
            const base = pole ? ICE : h > 0.78 ? HIGH : h > 0.67 ? LAND : h > 0.63 ? SHELF : SEA
            const shade = 0.12 + lit * 1.0
            const rim = Math.pow(1 - nz, 3) * 0.9 // air seen edge-on
            rgb = base.map((c, k) => Math.min(255, c * shade + ACCENT[k]! * rim * (0.3 + lit)))
        } else {
            // the air's glow just past the limb, then space
            const out = Math.sqrt(d2) - 1
            const glow = Math.max(0, 1 - out * 22) * 0.55
            const star = hash(x * 0.37, y * 0.71) > 0.996 ? 70 + hash(y, x) * 160 : 0
            const sky = [6 + y * 0.03, 10 + y * 0.04, 18 + y * 0.06]
            rgb = sky.map((c, k) => Math.min(255, c + ACCENT[k]! * glow + star))
        }
        image.data[i] = rgb[0]!; image.data[i + 1] = rgb[1]!; image.data[i + 2] = rgb[2]!; image.data[i + 3] = 255
    }
    context.putImageData(image, 0, 0)
})
</script>

<template>
    <canvas ref="canvas" class="backdrop" :width="W" :height="H" aria-hidden="true" />
</template>

<style scoped>
.backdrop { position: absolute; inset: 0; width: 100%; height: 100%; object-fit: cover; image-rendering: pixelated; }
</style>
