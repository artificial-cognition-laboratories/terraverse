<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { MatterElement, MatterStructure } from "../bridge"

/**
 * A substance's structure in 3D: its molecule, or a piece of its crystal.
 * Turns slowly; drag to turn it yourself. Atoms in their elements' colours,
 * sized by covalent radius.
 */
const props = defineProps<{ structure: MatterStructure; elements: Map<string, MatterElement> }>()

type Atom = { el: string; x: number; y: number; z: number }
type Bond = { a: number; b: number; order: number }

// A small piece of each lattice, in units of its cell, centred.
function lattice(kind: "fcc" | "bcc" | "diamond" | "rocksalt" | "layered", species: string[]): { atoms: Atom[]; reach: number } {
    const pts: [number, number, number, number][] = [] // x, y, z, species index
    const corners = [0, 1].flatMap((x) => [0, 1].flatMap((y) => [0, 1].map((z) => [x, y, z] as const)))
    switch (kind) {
        case "fcc":
        case "diamond":
            corners.forEach(([x, y, z]) => pts.push([x, y, z, 0]))
            for (const [x, y, z] of [[0.5, 0.5, 0], [0.5, 0.5, 1], [0.5, 0, 0.5], [0.5, 1, 0.5], [0, 0.5, 0.5], [1, 0.5, 0.5]])
                pts.push([x!, y!, z!, 0])
            if (kind === "diamond")
                for (const [x, y, z] of [[0.25, 0.25, 0.25], [0.75, 0.75, 0.25], [0.75, 0.25, 0.75], [0.25, 0.75, 0.75]])
                    pts.push([x!, y!, z!, 0])
            return { atoms: pts.map(([x, y, z, s]) => ({ el: species[s] ?? species[0]!, x: x - 0.5, y: y - 0.5, z: z - 0.5 })),
                reach: kind === "diamond" ? 0.44 : 0.72 }
        case "bcc":
            corners.forEach(([x, y, z]) => pts.push([x, y, z, 0]))
            pts.push([0.5, 0.5, 0.5, 0])
            return { atoms: pts.map(([x, y, z]) => ({ el: species[0]!, x: x - 0.5, y: y - 0.5, z: z - 0.5 })), reach: 0.88 }
        case "rocksalt":
            for (let x = 0; x <= 2; x++) for (let y = 0; y <= 2; y++) for (let z = 0; z <= 2; z++)
                pts.push([x / 2, y / 2, z / 2, (x + y + z) % 2])
            return { atoms: pts.map(([x, y, z, s]) => ({ el: species[s] ?? species[0]!, x: x - 0.5, y: y - 0.5, z: z - 0.5 })), reach: 0.51 }
        case "layered": {
            // two honeycomb sheets, offset as in graphite
            const atoms: Atom[] = []
            for (const [layer, shift] of [[-0.35, 0], [0.35, 0.29]] as const)
                for (let i = 0; i < 6; i++) {
                    const a = (i / 6) * Math.PI * 2
                    atoms.push({ el: species[0]!, x: Math.cos(a) * 0.29 + shift, y: layer, z: Math.sin(a) * 0.29 })
                }
            return { atoms: atoms, reach: 0.3 }
        }
    }
}

function model(): { atoms: Atom[]; bonds: Bond[]; scale: number } {
    const s = props.structure
    if (s.type === "molecule") {
        const atoms = s.atoms.map(([el, x, y, z]) => ({ el: el, x: x, y: y, z: z }))
        const span = Math.max(1, ...atoms.map((a) => Math.hypot(a.x, a.y, a.z)))
        return { atoms: atoms, bonds: s.bonds.map(([a, b, order]) => ({ a: a, b: b, order: order })), scale: 1 / span }
    }
    const { atoms, reach } = lattice(s.lattice, s.atoms)
    const bonds: Bond[] = []
    for (let i = 0; i < atoms.length; i++)
        for (let j = i + 1; j < atoms.length; j++) {
            const d = Math.hypot(atoms[i]!.x - atoms[j]!.x, atoms[i]!.y - atoms[j]!.y, atoms[i]!.z - atoms[j]!.z)
            if (d < reach) bonds.push({ a: i, b: j, order: 1 })
        }
    return { atoms: atoms, bonds: bonds, scale: 1.2 }
}

const canvas = ref<HTMLCanvasElement | null>(null)
let yaw = 0.6, pitch = 0.35, spin = true, frame = 0, drag: { x: number; y: number } | null = null

function draw() {
    const el = canvas.value
    if (!el) return
    const w = el.clientWidth, h = el.clientHeight, dpr = window.devicePixelRatio || 1
    el.width = Math.round(w * dpr); el.height = Math.round(h * dpr)
    const ctx = el.getContext("2d")!
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
    ctx.clearRect(0, 0, w, h)
    const { atoms, bonds, scale } = model()
    const size = Math.min(w, h) * 0.36 * scale
    const cy = Math.cos(yaw), sy = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch)
    const view = atoms.map((a) => {
        const x = a.x * cy - a.z * sy, z0 = a.x * sy + a.z * cy
        const y = a.y * cp - z0 * sp, z = a.y * sp + z0 * cp
        return { x: w / 2 + x * size, y: h / 2 - y * size, z: z, el: a.el }
    })
    for (const b of bonds) {
        const p = view[b.a]!, q = view[b.b]!
        const nx = -(q.y - p.y), ny = q.x - p.x, len = Math.hypot(nx, ny) || 1
        for (let k = 0; k < b.order; k++) {
            const off = (k - (b.order - 1) / 2) * 4
            ctx.strokeStyle = "#7b8790"
            ctx.lineWidth = 2.5
            ctx.beginPath()
            ctx.moveTo(p.x + (nx / len) * off, p.y + (ny / len) * off)
            ctx.lineTo(q.x + (nx / len) * off, q.y + (ny / len) * off)
            ctx.stroke()
        }
    }
    for (const a of [...view].sort((m, n) => m.z - n.z)) {
        const e = props.elements.get(a.el)
        const r = Math.max(5, (e?.radius ?? 120) / 140 * size * 0.22) * (1 + a.z * 0.15)
        const g = ctx.createRadialGradient(a.x - r * 0.35, a.y - r * 0.35, r * 0.1, a.x, a.y, r)
        g.addColorStop(0, "#ffffff")
        g.addColorStop(0.25, e?.color ?? "#b0b8c0")
        g.addColorStop(1, "#0b0f12")
        ctx.fillStyle = g
        ctx.beginPath(); ctx.arc(a.x, a.y, r, 0, Math.PI * 2); ctx.fill()
    }
}

function tick() {
    if (spin) yaw += 0.006
    draw()
    frame = requestAnimationFrame(tick)
}
function down(e: PointerEvent) { drag = { x: e.clientX, y: e.clientY }; spin = false; canvas.value?.setPointerCapture(e.pointerId) }
function move(e: PointerEvent) {
    if (!drag) return
    yaw += (e.clientX - drag.x) * 0.01
    pitch = Math.max(-1.4, Math.min(1.4, pitch + (e.clientY - drag.y) * 0.01))
    drag = { x: e.clientX, y: e.clientY }
}
watch(() => props.structure, () => { spin = true })
onMounted(() => { frame = requestAnimationFrame(tick) })
onBeforeUnmount(() => cancelAnimationFrame(frame))
</script>

<template>
    <canvas ref="canvas" class="structure" @pointerdown="down" @pointermove="move" @pointerup="drag = null"
        @pointercancel="drag = null" aria-label="The substance's structure in 3D" />
</template>

<style scoped>
.structure { display: block; width: 100%; height: 100%; cursor: grab; touch-action: none; }
.structure:active { cursor: grabbing; }
</style>
