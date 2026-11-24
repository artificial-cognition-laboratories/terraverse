<script setup lang="ts">
import { computed, ref } from "vue"

/** A choice on the inner ring; its `children` fan out on the outer ring while it's pointed at. */
export type RadialItem = { id: string; label: string; children?: readonly { id: string; label: string }[] }

/**
 * A radial menu: what to do, round the middle of the screen, glass as the hotbar's cells.
 * Point at a choice on the inner ring and its options fan out beyond it; click one to take it
 * (a choice with one option, or none, is taken itself). The middle is `none`. `chosen` is lit.
 */
const props = defineProps<{ items: readonly RadialItem[]; chosen?: { item: string; child: string | null } | null; none?: string }>()
const emit = defineEmits<{ pick: [item: string, child: string | null]; none: [] }>()

const R = { hole: 46, inner: 62, mid: 128, gap: 6, outer: 190 }
const hovered = ref<string | null>(null)
const span = computed(() => (Math.PI * 2) / Math.max(1, props.items.length))
// the first at the top, round clockwise
const angle = (i: number) => -Math.PI / 2 + i * span.value

function wedge(r0: number, r1: number, a0: number, a1: number) {
    const p = (r: number, a: number) => `${(r * Math.cos(a)).toFixed(2)} ${(r * Math.sin(a)).toFixed(2)}`
    const large = a1 - a0 > Math.PI ? 1 : 0
    return `M${p(r0, a0)} L${p(r1, a0)} A${r1} ${r1} 0 ${large} 1 ${p(r1, a1)} L${p(r0, a1)} A${r0} ${r0} 0 ${large} 0 ${p(r0, a0)}Z`
}
const at = (r: number, a: number) => ({ x: r * Math.cos(a), y: r * Math.sin(a) })
const PAD = 0.012 // radians between wedges: the cells' gap

const inner = computed(() => props.items.map((item, i) => {
    const a0 = angle(i) - span.value / 2, a1 = angle(i) + span.value / 2
    return { item: item, path: wedge(R.inner, R.mid, a0 + PAD, a1 - PAD), label: at((R.inner + R.mid) / 2, angle(i)) }
}))
// the pointed-at choice's options, fanned across its own wedge (wider where there are many)
const outer = computed(() => {
    const i = props.items.findIndex((it) => it.id === hovered.value)
    const children = props.items[i]?.children ?? []
    if (i < 0 || children.length < 2) return []
    const each = Math.max(span.value / children.length, 0.5)
    const start = angle(i) - (each * children.length) / 2
    return children.map((c, k) => {
        const a0 = start + k * each, a1 = a0 + each
        return { child: c, path: wedge(R.mid + R.gap, R.outer, a0 + PAD, a1 - PAD), label: at((R.mid + R.gap + R.outer) / 2, (a0 + a1) / 2) }
    })
})
function take(item: RadialItem) {
    const children = item.children ?? []
    if (children.length < 2) emit("pick", item.id, children[0]?.id ?? null)
}
const lit = (item: string, child: string | null = null) =>
    props.chosen?.item === item && (child === null || props.chosen.child === child)
</script>

<template>
    <svg class="radial" :viewBox="`${-R.outer - 2} ${-R.outer - 2} ${(R.outer + 2) * 2} ${(R.outer + 2) * 2}`" role="menu">
        <circle class="none" :class="{ on: !chosen }" :r="R.hole" role="menuitem" @click="emit('none')" @mouseenter="hovered = null" />
        <text v-if="none" class="label small" y="3">{{ none }}</text>
        <g v-for="w in inner" :key="w.item.id" role="menuitem" @mouseenter="hovered = w.item.id" @click="take(w.item)">
            <path class="cell" :class="{ over: hovered === w.item.id, on: lit(w.item.id) }" :d="w.path" />
            <text class="label" :x="w.label.x" :y="w.label.y + 3">{{ w.item.label }}</text>
        </g>
        <g v-for="w in outer" :key="w.child.id" role="menuitem" @click="emit('pick', hovered!, w.child.id)">
            <path class="cell" :class="{ on: lit(hovered!, w.child.id) }" :d="w.path" />
            <text class="label" :x="w.label.x" :y="w.label.y + 3">{{ w.child.label }}</text>
        </g>
    </svg>
</template>

<style scoped>
.radial { width: 24rem; height: 24rem; overflow: visible; }
/* the hotbar's cell (game/cell.css) on its strip's glass: a hairline, lit on hover, the ocean when chosen */
.cell, .none { fill: rgb(4 7 11 / 0.72); stroke: rgb(231 237 244 / 0.1); stroke-width: 1; cursor: pointer; }
.cell:hover, .cell.over, .none:hover { stroke: var(--edge-lit); fill: rgb(17 26 38 / 0.85); }
.cell.on, .none.on { stroke: var(--edge-lit-hi); fill: rgb(10 75 145 / 0.55); }
.label { fill: var(--ink); font: 9px var(--font-display); text-transform: uppercase; text-anchor: middle; pointer-events: none; }
.label.small { fill: var(--ink-faint); font-size: 7px; }
</style>
