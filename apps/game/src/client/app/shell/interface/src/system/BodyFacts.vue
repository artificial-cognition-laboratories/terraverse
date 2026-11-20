<script setup lang="ts">
import { computed } from "vue"
import type { BodyCard } from "../bridge"

/** What a body became, from the body model: its kind, air, warmth, field and make-up at a glance. */
const props = defineProps<{ card: BodyCard }>()

const GAS_COLOURS: Record<string, string> = { N2: "#6f8fb0", O2: "#59b4d8", CO2: "#c9a46a", CH4: "#c9824a", H2: "#d8c6a0", He: "#b8a6c8" }
const gases = computed(() => Object.entries(props.card.gases).filter(([, share]) => (share ?? 0) > 0.005)
    .sort((a, b) => (b[1] ?? 0) - (a[1] ?? 0)))
const celsius = (k: number) => `${Math.round(k - 273)} °C`

const air = computed(() => {
    const p = props.card.pressure
    if (p === null) return "no surface"
    if (p < 0.001) return "none"
    return p < 0.1 ? `${(p * 1000).toFixed(0)} mbar` : `${p.toFixed(p < 10 ? 2 : 0)} bar`
})
const water = computed(() => props.card.ocean ? "oceans"
    : props.card.kind === "ice shell" ? "ocean under ice"
    : props.card.temperature < 273 ? "ice, if any" : "dry")
const field = computed(() => props.card.field < 0.05 ? "none" : props.card.field < 0.4 ? "weak" : props.card.field < 2 ? "earthlike" : "strong")
const heat = computed(() => props.card.heat < 0.3 ? "cold, quiet" : props.card.heat < 3 ? "active" : props.card.heat < 8 ? "very active" : "molten")
const part = (v: number) => `${Math.round(v * 100)}%`
</script>

<template>
    <div class="facts">
        <header>
            <span class="kind">{{ card.kind }}</span>
            <span class="tags">
                <span v-if="card.locked">tidally locked</span>
                <span v-if="card.plates">plate tectonics</span>
                <span v-if="card.volcanism">volcanoes</span>
            </span>
        </header>
        <dl>
            <div><dt>Surface</dt><dd>{{ celsius(card.temperature) }}</dd></div>
            <div><dt>Without air</dt><dd>{{ celsius(card.equilibrium) }}</dd></div>
            <div><dt>Air</dt><dd>{{ air }}</dd></div>
            <div><dt>Water</dt><dd>{{ water }}</dd></div>
            <div><dt>Gravity</dt><dd>{{ card.gravity.toFixed(2) }} g</dd></div>
            <div><dt>Radius</dt><dd>{{ card.radius.toFixed(2) }} Earths</dd></div>
            <div><dt>Magnetic field</dt><dd>{{ field }}</dd></div>
            <div><dt>Interior</dt><dd>{{ heat }}{{ card.tidal > 0.3 ? ' (tides)' : '' }}</dd></div>
            <div><dt>Sunlight</dt><dd>{{ card.sunlight.toFixed(card.sunlight < 0.1 ? 3 : 2) }} × Earth</dd></div>
            <div><dt>Escape</dt><dd>{{ card.escape.toFixed(1) }} km/s</dd></div>
        </dl>
        <div v-if="gases.length" class="bar" :title="gases.map(([g, s]) => `${g} ${part(s ?? 0)}`).join(' · ')">
            <span v-for="[gas, share] in gases" :key="gas" :style="{ flexGrow: share, background: GAS_COLOURS[gas] }">
                <em v-if="(share ?? 0) > 0.12">{{ gas }} {{ part(share ?? 0) }}</em>
            </span>
        </div>
        <p class="makeup">metal {{ part(card.metal) }} · rock {{ part(card.rock) }} · ice {{ part(card.ice) }}{{ card.giant ? ' · the rest a deep gas envelope' : '' }}</p>
    </div>
</template>

<style scoped>
.facts { display: grid; gap: 10px; padding: 12px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk); }
header { display: flex; justify-content: space-between; align-items: baseline; gap: 10px; flex-wrap: wrap; }
.kind { font: 16px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
.tags { display: flex; gap: 8px; flex-wrap: wrap; font: 10px var(--font-display); text-transform: uppercase; color: var(--ink-faint); }
dl { display: grid; grid-template-columns: 1fr 1fr; gap: 4px 14px; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 8px; font-size: 11px; }
dt { color: var(--ink-dim); }
dd { margin: 0; color: var(--ink); font-family: var(--font-display); font-size: 11px; text-align: right; }
.bar { display: flex; height: 16px; border: var(--b) solid var(--edge); overflow: hidden; }
.bar span { display: grid; place-items: center; min-width: 2px; }
.bar em { font: normal 9px var(--font-display); color: #0b1014; white-space: nowrap; }
.makeup { margin: 0; font-size: 10px; color: var(--ink-faint); }
</style>
