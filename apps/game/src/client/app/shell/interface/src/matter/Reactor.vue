<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import type { MatterReaction, MatterState, MatterSubstance } from "../bridge"
import { useBridge } from "../bridge/use"
import { Button, Segmented, Slider } from "../kit"
import { PRESETS, type Preset } from "./presets"

/**
 * The Reactor: a vessel to fill, surroundings to set, and the engine
 * stepping the mixture live. What's in it, its temperature, and which
 * reactions are running (and how fast) all come back from `matter.react`;
 * the page only holds the mixture between steps.
 */
// `preset`: which set-up to load (the wiki's list picks it)
const props = defineProps<{ substances: MatterSubstance[]; reactions: MatterReaction[]; preset: string }>()
const bridge = useBridge()

const preset = ref<Preset>(PRESETS[0]!)
const contents = ref<Record<string, number>>({ ...preset.value.contents })
const temperature = ref(preset.value.surroundings)
const surroundings = ref(preset.value.surroundings)
const pressure = ref(preset.value.pressure)
const power = ref(preset.value.power)
const light = ref(preset.value.light)
const states = ref<Record<string, MatterState>>({})
const rates = ref<Record<string, number>>({})
const history = ref<number[]>([])
const elapsed = ref(0)
const running = ref(false)
const speed = ref(1)
let spark = false, timer = 0, busy = false

const SPEEDS = [1, 10, 100, 1000].map((s) => ({ value: s, label: `×${s}` }))
const TICK = 0.2 // seconds of real time between steps

function load(p: Preset) {
    preset.value = p
    contents.value = { ...p.contents }
    temperature.value = surroundings.value = p.surroundings
    pressure.value = p.pressure
    power.value = p.power
    light.value = p.light
    rates.value = {}
    history.value = []
    elapsed.value = 0
    step(0) // learn the states without running time
}

async function step(seconds: number) {
    if (busy) return
    busy = true
    try {
        const out = await bridge.send("matter.react", {
            amounts: contents.value, temperature: temperature.value, surroundings: surroundings.value,
            pressure: pressure.value, power: power.value, light: light.value, seconds: seconds, spark: spark,
        })
        spark = false
        contents.value = out.amounts
        temperature.value = out.temperature
        states.value = out.states
        rates.value = out.rates
        elapsed.value += seconds
        if (seconds > 0) history.value = [...history.value.slice(-149), out.temperature]
    } finally {
        busy = false
    }
}

function toggle() {
    running.value = !running.value
    window.clearInterval(timer)
    if (running.value) timer = window.setInterval(() => step(TICK * speed.value), TICK * 1000)
}
function strike() {
    spark = true
    if (!running.value) step(TICK * speed.value)
}
onBeforeUnmount(() => window.clearInterval(timer))
function presetById(id: string): Preset {
    const p = PRESETS.find((x) => x.id === id)
    if (!p) throw new Error(`no Reactor preset "${id}"`)
    return p
}
watch(() => props.preset, (id) => load(presetById(id)), { immediate: true })

// what's in the vessel, most first, with a colour each
const bySubstance = computed(() => new Map(props.substances.map((s) => [s.id, s])))
function hue(id: string) {
    let h = 0
    for (const c of id) h = (h * 31 + c.charCodeAt(0)) % 360
    return `hsl(${h} 55% 55%)`
}
const rows = computed(() => {
    const total = Object.values(contents.value).reduce((a, b) => a + b, 0) || 1
    return Object.entries(contents.value).filter(([, n]) => n > 1e-6).sort((a, b) => b[1] - a[1])
        .map(([id, n]) => ({ id: id, n: n, share: n / total, colour: hue(id), name: bySubstance.value.get(id)?.name ?? id,
            formula: bySubstance.value.get(id)?.formula ?? id, state: states.value[id] }))
})
const addable = computed(() => props.substances.filter((s) => !(contents.value[s.id] && contents.value[s.id]! > 0)))
const adding = ref("")
function add() {
    if (!adding.value) return
    contents.value = { ...contents.value, [adding.value]: 1 }
    adding.value = ""
    step(0)
}
function setAmount(id: string, n: number) {
    contents.value = { ...contents.value, [id]: n }
}
function typed(id: string, event: Event) {
    if (!(event.target instanceof HTMLInputElement)) throw new Error("an amount came from something that isn't an input")
    setAmount(id, Math.max(0, Number(event.target.value)))
}
function remove(id: string) {
    const next = { ...contents.value }
    delete next[id]
    contents.value = next
}

// the temperature over time, as a line
const line = computed(() => {
    const h = history.value
    if (h.length < 2) return ""
    const lo = Math.min(...h, surroundings.value) - 10, hi = Math.max(...h, surroundings.value) + 10
    return h.map((t, i) => `${(i / (h.length - 1)) * 100},${40 - ((t - lo) / (hi - lo)) * 40}`).join(" ")
})

const formula = (id: string) => bySubstance.value.get(id)?.formula ?? id
function side(terms: Record<string, number>) {
    return Object.entries(terms).map(([id, n]) => `${n === 1 ? "" : `${n} `}${formula(id)}`).join(" + ")
}
const sorted = computed(() => [...props.reactions].sort((a, b) => (rates.value[b.id] ?? 0) - (rates.value[a.id] ?? 0)))
const log = (v: number) => Math.log10(Math.max(v, 1e-12))
const kelvin = (k: number) => `${Math.round(k)} K · ${Math.round(k - 273.15)} °C`
const pascals = (p: number) => p >= 1e6 ? `${(p / 1e6).toPrecision(3)} MPa` : p >= 1e3 ? `${(p / 1e3).toPrecision(3)} kPa` : `${p.toPrecision(3)} Pa`
const clock = (s: number) => s < 120 ? `${s.toFixed(1)} s` : s < 7200 ? `${(s / 60).toFixed(1)} min` : s < 172800 ? `${(s / 3600).toFixed(1)} h` : `${(s / 86400).toFixed(1)} days`
</script>

<template>
    <div class="reactor">
        <section class="vessel">
            <p class="hint">{{ preset.hint }}</p>

            <div class="readout">
                <div class="temp"><span class="label">Inside</span><strong>{{ kelvin(temperature) }}</strong></div>
                <svg class="spark-line" viewBox="0 0 100 40" preserveAspectRatio="none"><polyline :points="line" /></svg>
                <div class="temp"><span class="label">Time</span><strong>{{ clock(elapsed) }}</strong></div>
            </div>

            <div class="bar">
                <span v-for="r in rows" :key="r.id" :style="{ flexGrow: r.share, background: r.colour }" :title="`${r.name} ${r.n.toPrecision(3)} mol`" />
            </div>
            <ul class="contents">
                <li v-for="r in rows" :key="r.id">
                    <span class="dot" :style="{ background: r.colour }" />
                    <span class="formula">{{ r.formula }}</span>
                    <span class="name">{{ r.name }}</span>
                    <span class="state" :class="r.state">{{ r.state }}</span>
                    <input type="number" min="0" step="0.1" :value="+r.n.toPrecision(4)" :disabled="running"
                        @change="typed(r.id, $event)" />
                    <span class="unit">mol</span>
                    <button v-if="!running" class="x" @click="remove(r.id)">✕</button>
                </li>
            </ul>
            <div v-if="!running" class="add">
                <select v-model="adding">
                    <option value="">add a substance…</option>
                    <option v-for="s in addable" :key="s.id" :value="s.id">{{ s.formula }} · {{ s.name }}</option>
                </select>
                <Button size="sm" :disabled="!adding" @click="add">Add</Button>
            </div>

            <div class="conditions">
                <Slider :model-value="log(surroundings)" label="Surroundings" :min="1.5" :max="3.6" :step="0.005"
                    :format="(v) => kelvin(10 ** v)" @update:model-value="surroundings = 10 ** $event" />
                <Slider :model-value="log(pressure)" label="Pressure" :min="0" :max="8" :step="0.01"
                    :format="(v) => pascals(10 ** v)" @update:model-value="pressure = 10 ** $event" />
                <Slider v-model="power" label="Electric power" :min="0" :max="5000" :step="50" :format="(v) => `${v} W`" />
                <Slider v-model="light" label="Light" :min="0" :max="1000" :step="10" :format="(v) => `${v} W`" />
            </div>

            <div class="run">
                <Button variant="primary" @click="toggle">{{ running ? "Pause" : "Run" }}</Button>
                <Button @click="strike">Strike a spark</Button>
                <Button @click="load(preset)">Reset</Button>
                <Segmented v-model="speed" :options="SPEEDS" class="speed" @update:model-value="running && (toggle(), toggle())" />
            </div>
        </section>

        <aside class="reactions">
            <h3>Reactions</h3>
            <article v-for="r in sorted" :key="r.id" class="reaction" :class="{ live: (rates[r.id] ?? 0) > 1e-9 }">
                <header>
                    <strong>{{ r.name }}</strong>
                    <span v-if="(rates[r.id] ?? 0) > 1e-9" class="rate">{{ rates[r.id]!.toPrecision(2) }} /s</span>
                </header>
                <p class="equation">{{ side(r.reactants) }} → {{ side(r.products) }}</p>
                <p class="meta">
                    {{ r.heat > 0 ? `gives ${r.heat} kJ` : `takes ${-r.heat} kJ` }}
                    <template v-if="r.energy"> · needs {{ r.energy }}</template>
                    <template v-else> · activation {{ r.activation }} kJ/mol</template>
                    <template v-for="q in r.requires ?? []" :key="q.substance"> · {{ formula(q.substance) }} {{ q.state }}</template>
                </p>
                <p class="note">{{ r.note }}</p>
            </article>
        </aside>
    </div>
</template>

<style scoped>
.reactor { display: grid; grid-template-columns: minmax(0, 1fr) 380px; gap: 0 22px; height: 100%; min-height: 0; }
.vessel { display: flex; flex-direction: column; gap: 10px; min-height: 0; overflow-y: auto; padding-right: 6px; }
.presets { display: flex; flex-wrap: wrap; gap: 6px; }
.presets button { border: var(--b) solid var(--edge); background: var(--stone-2); padding: 4px 8px; font: 10px var(--font-display); text-transform: uppercase; color: var(--ink-dim); }
.presets button.on { background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.hint { margin: 0; font-size: 12px; color: var(--ink-dim); }

.readout { display: grid; grid-template-columns: auto 1fr auto; gap: 14px; align-items: center; padding: 8px 12px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk); }
.temp { display: grid; gap: 2px; }
.label { font: 10px var(--font-display); text-transform: uppercase; color: var(--ink-faint); }
.temp strong { font: 14px var(--font-display); }
.spark-line { width: 100%; height: 40px; }
.spark-line polyline { fill: none; stroke: #ff9a4a; stroke-width: 1.5; vector-effect: non-scaling-stroke; }

.bar { display: flex; height: 18px; border: var(--b) solid var(--edge); overflow: hidden; }
.bar span { min-width: 1px; transition: flex-grow 200ms; }
.contents { list-style: none; margin: 0; padding: 0; display: grid; gap: 3px; }
.contents li { display: grid; grid-template-columns: 10px 64px 1fr 90px 72px 26px 16px; gap: 8px; align-items: center; font-size: 12px; }
.dot { width: 10px; height: 10px; }
.formula { font: 12px var(--font-display); }
.name { color: var(--ink-dim); }
.state { font: 10px var(--font-display); text-transform: uppercase; }
.solid { color: #8fb6e8; } .liquid { color: #40b8ec; } .gas { color: #d9b07a; } .supercritical { color: #c99be0; }
.contents input { width: 100%; padding: 2px 4px; border: var(--b) solid var(--edge); background: var(--stone-0); color: var(--ink); font: 11px var(--font-display); }
.unit { color: var(--ink-faint); font-size: 10px; }
.x { border: 0; background: none; padding: 0; color: var(--ink-faint); }
.add { display: flex; gap: 8px; }
.add select { flex: 1; padding: 4px; border: var(--b) solid var(--edge); background: var(--stone-1); color: var(--ink); font-size: 12px; }
.conditions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px 18px; }
.run { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
.speed { width: 220px; margin-left: auto; }

.reactions { display: flex; flex-direction: column; gap: 8px; min-height: 0; overflow-y: auto; padding-left: 22px; border-left: var(--b) solid var(--edge); }
h3 { margin: 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.reaction { padding: 8px 10px; border: var(--b) solid var(--edge); background: var(--stone-1); opacity: 0.6; }
.reaction.live { opacity: 1; border-color: var(--accent); box-shadow: inset 2px 0 0 var(--accent-hi); }
.reaction header { display: flex; justify-content: space-between; gap: 8px; }
.reaction strong { font: 12px var(--font-display); text-transform: uppercase; }
.rate { font: 11px var(--font-display); color: var(--accent-hi); }
.equation { margin: 4px 0; font: 12px var(--font-display); color: var(--ink); }
.meta, .note { margin: 0; font-size: 10px; color: var(--ink-faint); line-height: 1.4; }
.note { color: var(--ink-dim); margin-top: 3px; }
</style>
