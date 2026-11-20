<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { MatterState } from "../bridge"

/**
 * A box of particles acting out a state: held in a lattice and shivering
 * (solid), sliding over each other in a pool (liquid), flying and bouncing
 * (gas), or a dense, restless fluid (supercritical). Hotter shivers and
 * flies faster. A picture of the state, not a simulation of it.
 */
const props = defineProps<{ state: MatterState; heat: number; color: string }>() // heat: 0..1, how hot for its state

type P = { x: number; y: number; vx: number; vy: number; hx: number; hy: number }
const N = 49
const canvas = ref<HTMLCanvasElement | null>(null)
let parts: P[] = []
let frame = 0

function reset() {
    const el = canvas.value
    const aspect = el && el.clientHeight ? el.clientWidth / el.clientHeight : 1.5
    // a square lattice in pixels, as big as fits: rows and columns the same distance apart
    const gy = Math.min(0.09 * aspect, 0.8 / 6), gx = gy / aspect
    parts = Array.from({ length: N }, (_, i) => {
        const hx = 0.5 + ((i % 7) - 3) * gx, hy = 0.9 - (6 - Math.floor(i / 7)) * gy
        return { x: hx, y: hy, vx: (Math.random() - 0.5) * 0.01, vy: (Math.random() - 0.5) * 0.01, hx: hx, hy: hy }
    })
}

function step() {
    const s = props.state, heat = 0.3 + props.heat
    for (const p of parts) {
        if (s === "solid") {
            p.x = p.hx + (Math.random() - 0.5) * 0.012 * heat
            p.y = p.hy + (Math.random() - 0.5) * 0.012 * heat
            continue
        }
        const speed = s === "gas" ? 0.012 * heat : s === "supercritical" ? 0.006 * heat : 0.003 * heat
        p.vx += (Math.random() - 0.5) * speed * 0.6
        p.vy += (Math.random() - 0.5) * speed * 0.6 + (s === "liquid" ? 0.0006 : 0)
        const v = Math.hypot(p.vx, p.vy)
        if (v > speed) { p.vx *= speed / v; p.vy *= speed / v }
        p.x += p.vx; p.y += p.vy
        const top = s === "liquid" ? 0.45 : 0.04
        if (p.x < 0.04 || p.x > 0.96) { p.vx = -p.vx; p.x = Math.max(0.04, Math.min(0.96, p.x)) }
        if (p.y < top || p.y > 0.96) { p.vy = -p.vy; p.y = Math.max(top, Math.min(0.96, p.y)) }
    }
}

function draw() {
    const el = canvas.value
    if (!el) return
    const w = el.clientWidth, h = el.clientHeight, dpr = window.devicePixelRatio || 1
    el.width = Math.round(w * dpr); el.height = Math.round(h * dpr)
    const ctx = el.getContext("2d")!
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
    ctx.clearRect(0, 0, w, h)
    ctx.strokeStyle = "#3a434b"; ctx.lineWidth = 2
    ctx.strokeRect(1, 1, w - 2, h - 2)
    ctx.fillStyle = props.color
    for (const p of parts) {
        ctx.beginPath(); ctx.arc(p.x * w, p.y * h, Math.max(2.5, w / 50), 0, Math.PI * 2); ctx.fill()
    }
}

function tick() { step(); draw(); frame = requestAnimationFrame(tick) }
watch(() => props.state, (now, was) => { if (now === "solid" && was !== "solid") reset() })
onMounted(() => { reset(); frame = requestAnimationFrame(tick) })
onBeforeUnmount(() => cancelAnimationFrame(frame))
</script>

<template>
    <canvas ref="canvas" class="box" :aria-label="`Particles as a ${state}`" />
</template>

<style scoped>
.box { display: block; width: 100%; height: 100%; }
</style>
