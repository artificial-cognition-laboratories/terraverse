<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useStore } from "./store"

/** What the probe (or the heat gun) last read, bottom left of the HUD for a few seconds: the block, its matter, temperature and state. */
const store = useStore()
const SHOWN = 6000 // ms
const visible = ref(false)
let timer = 0
watch(() => store.ui.probe, (r) => {
    if (!r) return
    visible.value = true
    clearTimeout(timer)
    timer = window.setTimeout(() => { visible.value = false }, SHOWN)
})
const r = computed(() => store.ui.probe)
const name = (id?: string) => (id ?? "").replace(/_/g, " ")
const moles = (n: number) => n >= 1 ? `${n.toPrecision(3)} mol` : `${(n * 1000).toPrecision(3)} mmol`
const kelvin = (t: number) => `${Math.round(t)} K · ${Math.round(t - 273.15)} °C`
</script>

<template>
    <div v-if="visible && r" class="probe">
        <span class="node"><span v-if="r.colour" class="swatch" :style="{ background: r.colour }" />{{ r.node }}</span>
        <template v-if="r.empty"><span class="dim">no matter registered</span></template>
        <template v-else>
            <span v-if="r.substance" class="what">{{ name(r.substance) }} · {{ r.state }} · {{ Math.round((r.fill ?? 0) * 100) }}% of the block</span>
            <span v-else class="what">gas</span>
            <span class="temp">{{ r.temperature !== undefined ? kelvin(r.temperature) : "" }}</span>
            <span v-if="r.activity && Math.abs(r.activity) > 0.5" class="dim">{{ r.activity > 0 ? "giving off" : "taking in" }} {{ Math.abs(Math.round(r.activity)) }} W</span>
            <span v-if="r.vapour && (r.haze ?? 0) > 0.02" class="dim">{{ name(r.vapour) }} {{ Math.round((r.haze ?? 0) * 100) }}% of its gas</span>
            <span v-for="c in r.contents ?? []" :key="c.substance" class="dim">{{ name(c.substance) }} · {{ c.state }} · {{ moles(c.moles) }}</span>
            <span class="dim">{{ r.awake ? "awake" : "settled" }}</span>
            <template v-if="r.river">
                <span v-if="r.river.lake" class="what">a lake · stands at its level</span>
                <template v-else>
                    <span class="what">river · {{ Math.round(r.river.discharge) }} m³/s · ×{{ r.river.ratio.toFixed(2) }} its usual</span>
                    <span class="dim">{{ r.river.local.toFixed(1) }} m³/s in its channel here</span>
                    <span class="dim">{{ r.river.rise >= 0 ? "+" : "" }}{{ r.river.rise.toFixed(2) }} blocks · {{ r.river.speed.toFixed(2) }} blocks/s · {{ r.river.to_sea.toFixed(1) }} days to the sea</span>
                </template>
            </template>
        </template>
    </div>
</template>

<style scoped>
.probe { position: absolute; left: 14px; bottom: 14px; display: flex; flex-direction: column; gap: 3px; padding: 8px 12px;
    border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.86); box-shadow: var(--raised); font-size: 12px; color: var(--ink); max-width: 360px; }
.node { font: 11px var(--font-display); text-transform: uppercase; color: var(--ink-faint); }
.what { font: 13px var(--font-display); }
.temp { color: var(--accent-hi); font-family: var(--font-display); }
.dim { color: var(--ink-dim); font-size: 11px; }
.swatch { display: inline-block; width: 10px; height: 10px; margin-right: 6px; border: 1px solid var(--edge); vertical-align: -1px; }
</style>
