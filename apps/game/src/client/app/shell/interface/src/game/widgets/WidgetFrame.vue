<script setup lang="ts">
import type { WidgetCorner } from "./config"

defineProps<{ title: string; corner: WidgetCorner; interactive?: boolean; belowProfile?: boolean; bare?: boolean }>()
</script>

<template>
    <section class="widget-frame" :class="[`corner-${corner}`, { interactive, 'below-profile': belowProfile, bare }]" :aria-label="title">
        <header v-if="!bare"><span class="mark">»</span> {{ title }}</header>
        <div class="widget-body"><slot /></div>
    </section>
</template>

<style scoped>
.widget-frame { position: fixed; z-index: 2; min-width: 0; border: var(--b) solid var(--edge); background: rgb(14 18 21 / .92); box-shadow: var(--raised), 5px 5px 0 rgb(0 0 0 / .3); color: var(--ink); pointer-events: none; }
.widget-frame.interactive { pointer-events: auto; }
.widget-frame header { padding: 7px 9px; border-bottom: var(--b) solid var(--edge); background: var(--stone-2); font: 10px var(--font-display); text-transform: uppercase; letter-spacing: .1em; white-space: nowrap; }
.mark { color: var(--accent-hi); }
.widget-body { padding: 9px; }
.corner-0 { top: 16px; left: 16px; }
.corner-1 { top: 16px; right: 16px; }
.corner-2 { bottom: 16px; left: 16px; }
.corner-3 { bottom: 16px; right: 16px; }
.corner-1.below-profile { top: 86px; }
.bare { border: 0; background: none; box-shadow: none; }
.bare .widget-body { padding: 0; }
</style>
