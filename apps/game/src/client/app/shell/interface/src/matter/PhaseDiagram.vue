<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { MatterSubstance } from "../bridge"
import { ENVIRONMENTS } from "./environments"

/**
 * The phase diagram: temperature across, pressure up (log). Regions are
 * bounded by the engine's own curves (sublimation, boiling, melting) and
 * the critical point; the reference worlds are marked. Click or drag to
 * move the marker; the screen asks the engine what the substance is there.
 */
const props = defineProps<{ substance: MatterSubstance }>()
const marker = defineModel<{ t: number; p: number }>({ required: true })

const canvas = ref<HTMLCanvasElement | null>(null)
let observer: ResizeObserver | null = null
let dragging = false

const range = computed(() => {
    const s = props.substance
    const tmin = s.triple[0] * 0.5
    const tmax = Math.max(s.critical[0] * 1.25, s.triple[0] * 2)
    const pmin = Math.max(1e-8, Math.min(...s.curves.sublimation.map(([, p]) => p)))
    return { tmin: tmin, tmax: tmax, lmin: Math.log10(pmin), lmax: 10 }
})
const PAD = { l: 46, r: 10, t: 10, b: 28 }

function toScreen(t: number, p: number, w: number, h: number) {
    const r = range.value
    const x = PAD.l + ((t - r.tmin) / (r.tmax - r.tmin)) * (w - PAD.l - PAD.r)
    const y = h - PAD.b - ((Math.log10(p) - r.lmin) / (r.lmax - r.lmin)) * (h - PAD.t - PAD.b)
    return [x, y] as const
}
function fromScreen(x: number, y: number, w: number, h: number) {
    const r = range.value
    const t = r.tmin + ((x - PAD.l) / (w - PAD.l - PAD.r)) * (r.tmax - r.tmin)
    const l = r.lmin + ((h - PAD.b - y) / (h - PAD.t - PAD.b)) * (r.lmax - r.lmin)
    return { t: Math.max(1, Math.min(r.tmax, t)), p: 10 ** Math.max(r.lmin, Math.min(r.lmax, l)) }
}

const REGIONS = { solid: "rgb(110 150 200 / 0.28)", liquid: "rgb(64 184 236 / 0.24)", gas: "rgb(200 170 120 / 0.14)", supercritical: "rgb(190 120 220 / 0.18)" }

function draw() {
    const el = canvas.value
    if (!el) return
    const w = el.clientWidth, h = el.clientHeight, dpr = window.devicePixelRatio || 1
    el.width = Math.round(w * dpr); el.height = Math.round(h * dpr)
    const ctx = el.getContext("2d")!
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
    ctx.clearRect(0, 0, w, h)
    const s = props.substance, r = range.value
    const P = (t: number, p: number) => toScreen(t, Math.max(10 ** r.lmin, Math.min(1e10, p)), w, h)
    const sub = s.curves.sublimation, vap = s.curves.vapour, melt = s.curves.melting
    const [tc, pc] = s.critical, top = 1e10, bottom = 10 ** r.lmin

    const region = (fill: string, pts: (readonly [number, number])[]) => {
        ctx.fillStyle = fill
        ctx.beginPath()
        pts.forEach(([t, p], i) => { const [x, y] = P(t, p); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y) })
        ctx.closePath(); ctx.fill()
    }
    // regions, bounded by the engine's curves
    region(REGIONS.gas, [[r.tmin, bottom], [r.tmax, bottom], [r.tmax, pc], [tc, pc], ...[...vap].reverse(), ...[...sub].reverse()])
    region(REGIONS.solid, [[r.tmin, top], [r.tmin, sub[0]![1]], ...sub, ...melt, [melt[melt.length - 1]![0], top]])
    region(REGIONS.liquid, [...melt, [tc, top], [tc, pc], ...[...vap].reverse()])
    region(REGIONS.supercritical, [[tc, pc], [r.tmax, pc], [r.tmax, top], [tc, top]])

    ctx.strokeStyle = "#cfd8de"; ctx.lineWidth = 1.5
    for (const curve of [sub, vap, melt]) {
        ctx.beginPath()
        curve.forEach(([t, p], i) => { const [x, y] = P(t, p); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y) })
        ctx.stroke()
    }

    // axes: decades of pressure, temperatures in kelvin
    ctx.font = '10px "JetBrains Mono", monospace'
    ctx.fillStyle = "#8a959d"; ctx.textAlign = "right"
    for (let l = Math.ceil(r.lmin); l <= r.lmax; l += Math.max(1, Math.round((r.lmax - r.lmin) / 6))) {
        const [, y] = toScreen(r.tmin, 10 ** l, w, h)
        ctx.fillText(l >= 9 ? `${10 ** (l - 9)} GPa` : l >= 6 ? `${10 ** (l - 6)} MPa` : l >= 3 ? `${10 ** (l - 3)} kPa` : l >= 0 ? `${10 ** l} Pa` : `1e${l} Pa`, PAD.l - 4, y + 3)
    }
    const [, atm] = toScreen(r.tmin, 101325, w, h)
    ctx.setLineDash([3, 4]); ctx.strokeStyle = "#56616a"
    ctx.beginPath(); ctx.moveTo(PAD.l, atm); ctx.lineTo(w - PAD.r, atm); ctx.stroke(); ctx.setLineDash([])
    ctx.textAlign = "center"
    // a round step giving at most six labels: 1, 2 or 5 times a power of ten
    const raw = (r.tmax - r.tmin) / 6, decade = Math.pow(10, Math.floor(Math.log10(raw)))
    const step = [1, 2, 5, 10].map((m) => m * decade).find((s) => s >= raw)!
    for (let t = Math.ceil(r.tmin / step) * step; t <= r.tmax; t += step) {
        const [x] = toScreen(t, 1, w, h)
        ctx.fillText(`${Math.round(t)} K`, x, h - 10)
    }

    // the triple and critical points
    for (const [[t, p], label] of [[s.triple, "triple"], [s.critical, "critical"]] as const) {
        const [x, y] = P(t, p)
        ctx.fillStyle = "#e8ecef"; ctx.beginPath(); ctx.arc(x, y, 3.5, 0, Math.PI * 2); ctx.fill()
        ctx.textAlign = "left"; ctx.fillText(label, x + 6, y - 4)
    }
    // reference worlds
    for (const env of ENVIRONMENTS) {
        if (env.t < r.tmin || env.t > r.tmax || Math.log10(env.p) < r.lmin) continue
        const [x, y] = P(env.t, env.p)
        ctx.fillStyle = "#ff9a4a"
        ctx.beginPath(); ctx.moveTo(x, y - 4); ctx.lineTo(x + 4, y); ctx.lineTo(x, y + 4); ctx.lineTo(x - 4, y); ctx.fill()
        ctx.fillStyle = "#d9a777"; ctx.textAlign = "left"; ctx.fillText(env.label, x + 6, y + 3)
    }
    // the marker
    const [mx, my] = P(marker.value.t, marker.value.p)
    ctx.strokeStyle = "#40b8ec"; ctx.lineWidth = 1
    ctx.beginPath(); ctx.moveTo(mx, PAD.t); ctx.lineTo(mx, h - PAD.b); ctx.moveTo(PAD.l, my); ctx.lineTo(w - PAD.r, my); ctx.stroke()
    ctx.fillStyle = "#40b8ec"; ctx.beginPath(); ctx.arc(mx, my, 5, 0, Math.PI * 2); ctx.fill()
}

function place(e: PointerEvent) {
    const el = canvas.value!
    const rect = el.getBoundingClientRect()
    marker.value = fromScreen(e.clientX - rect.left, e.clientY - rect.top, rect.width, rect.height)
}
function down(e: PointerEvent) { dragging = true; canvas.value?.setPointerCapture(e.pointerId); place(e) }
function move(e: PointerEvent) { if (dragging) place(e) }

watch(() => [props.substance, marker.value], draw, { deep: true })
onMounted(() => { observer = new ResizeObserver(draw); observer.observe(canvas.value!); draw() })
onBeforeUnmount(() => observer?.disconnect())

/** Put the marker on a reference world. */
function goTo(t: number, p: number) { marker.value = { t: t, p: p } }
defineExpose({ goTo })
</script>

<template>
    <canvas ref="canvas" class="diagram" @pointerdown="down" @pointermove="move" @pointerup="dragging = false"
        @pointercancel="dragging = false" aria-label="Phase diagram: drag to choose a temperature and pressure" />
</template>

<style scoped>
.diagram { display: block; width: 100%; height: 100%; cursor: crosshair; touch-action: none; }
</style>
