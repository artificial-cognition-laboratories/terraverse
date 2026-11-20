<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, reactive, ref, shallowRef, watch } from "vue"
import type { FloraGene, FloraSite, FloraState, SceneRect } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { Button, Field, ListGroup, ListItem, Segmented, Slider, Toggle, WorkbenchColumns } from "../kit/index.ts"
import { FLORA_PLACES, LIMITS, STRATEGIES } from "./flora.ts"

/** A plant grown by the game's own generator: a world's lineage, its genes set by hand if you like, at the site set here. */
const bridge = useBridge()
const world = ref("terra")
const lineage = ref(0)
const plant = ref(1)
const age = ref(0.6)
const season = ref(0.5)
const genes = reactive<Record<string, number>>({}) // set by hand, over the lineage's
const tab = ref<"genes" | "site">("genes")
const tabs = [{ value: "genes" as const, label: "Genes" }, { value: "site" as const, label: "Site" }]
const place = ref<string | null>(FLORA_PLACES[0]!.id)
const site = reactive<FloraSite>({ ...FLORA_PLACES[0]!.site })
const state = shallowRef<FloraState | null>(null)
const problem = ref<string | null>(null)
const host = ref<HTMLElement | null>(null)
const card = ref<HTMLElement | null>(null)
const fail = (err: unknown) => { problem.value = err instanceof Error ? err.message : String(err) }
const rect = (): SceneRect | null => {
    const r = host.value?.getBoundingClientRect()
    return r && r.width > 0 ? { x: r.left, y: r.top, width: r.width, height: r.height } : null
}
const right = () => card.value ? card.value.getBoundingClientRect().width + 24 : 0
let shown = false
async function set() {
    if (!shown) return
    state.value = await bridge.send("flora.set", { world: world.value, lineage: lineage.value, plant: plant.value, life: { age: age.value, season: season.value }, genes: { ...genes }, site: { ...site } })
    problem.value = null
}
async function show() {
    const where = rect()
    if (!where) return
    await bridge.send("flora.show", { rect: where, right: right() })
    shown = true
    await set()
}
function go(id: string) {
    const to = FLORA_PLACES.find((p) => p.id === id)
    if (!to) throw new Error(`no flora place ${id}`)
    Object.assign(site, to.site)
    place.value = id
}
watch([world, lineage], () => { for (const id of Object.keys(genes)) delete genes[id] })
watch([world, lineage, plant, age, season], () => { set().catch(fail) })
watch(genes, () => { set().catch(fail) })
watch(site, () => { set().catch(fail) })
const sizes = new ResizeObserver(() => {
    const where = rect()
    if (where) bridge.send("scene.place", { rect: where, right: right() }).catch(fail)
})
onMounted(() => {
    if (host.value) sizes.observe(host.value)
    if (card.value) sizes.observe(card.value)
    show().catch(fail)
})
onBeforeUnmount(() => {
    sizes.disconnect()
    shown = false
    bridge.send("scene.hide", {}).catch(fail)
})
const signed = (v: number) => v.toFixed(2)
const groups = (list: { group: string }[]) => [...new Set(list.map((x) => x.group))]
const geneGroups = computed(() => groups(state.value?.genes ?? []))
const lineageGroups = computed(() => groups(state.value?.lineages ?? []))
const shown_ = (g: FloraGene) => genes[g.id] ?? g.value
const step = (g: FloraGene) => g.whole ? 1 : (g.hi - g.lo) / 100
const geneText = (g: FloraGene) => (v: number) => {
    const at = g.whole ? String(Math.round(v)) : Math.abs(v) >= 10 ? v.toFixed(0) : v.toFixed(2)
    const grown = g.whole ? String(Math.round(g.grown)) : Math.abs(g.grown) >= 10 ? g.grown.toFixed(0) : g.grown.toFixed(2)
    return grown === at ? at : `${at} → ${grown}`
}
const ageOf = (v: number) => v < 0.2 ? "seedling" : v < 0.45 ? "young" : v < 0.8 ? "mature" : "ancient"
const seasonOf = (v: number) => ["midwinter", "late winter", "spring", "early summer", "midsummer", "late summer", "autumn", "early winter"][Math.min(7, Math.floor(v * 8 + 0.5) % 8)]!
const share = (v: number) => `${Math.round(v * 100)}%`
const worth = (b: FloraState["blocks"]) => Object.entries(b).filter(([, n]) => n > 0)
    .map(([part, n]) => `${n < 10 ? n.toFixed(1) : Math.round(n)} ${part}`).join(", ")
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <ListGroup v-for="group in lineageGroups" :key="group" :title="group">
                <template v-for="(l, i) in state?.lineages ?? []" :key="i">
                    <ListItem v-if="l.group === group" :on="lineage === i" @click="lineage = i">
                        <span class="swatch" :style="{ background: l.leaf }" /><span class="swatch" :style="{ background: l.bark }" />
                        <span :class="{ dead: !l.lives }">{{ l.name }} <small>{{ l.preset }}</small></span>
                    </ListItem>
                </template>
            </ListGroup>
            <ListGroup title="Places">
                <ListItem v-for="p in FLORA_PLACES" :key="p.id" :on="place === p.id" @click="go(p.id)">{{ p.title }}</ListItem>
            </ListGroup>
        </template>
        <template #content>
            <div ref="host" class="scene">
                <aside ref="card" class="facts">
                    <Segmented v-model="tab" :options="tabs" />
                    <template v-if="tab === 'genes'">
                        <p class="hint">Its own on the left of the arrow, what this site grew of it on the right.</p>
                        <Button size="sm" :disabled="!Object.keys(genes).length" @click="Object.keys(genes).forEach((id) => delete genes[id])">Back to its own genes</Button>
                        <template v-for="group in geneGroups" :key="group">
                            <h5>{{ group }}</h5>
                            <template v-for="g in state?.genes ?? []" :key="g.id">
                                <Slider v-if="g.group === group" :model-value="shown_(g)" :label="g.label" :min="g.lo" :max="g.hi" :step="step(g)"
                                    :format="geneText(g)" :class="{ set: g.id in genes }" @update:model-value="(v: number) => { genes[g.id] = v }" />
                            </template>
                        </template>
                    </template>
                    <template v-else>
                        <h5>This plant</h5>
                        <Slider v-model="age" label="Age" :min="0" :max="1" :step="0.05" :format="ageOf" />
                        <Slider v-model="season" label="Time of year" :min="0" :max="0.95" :step="0.05" :format="seasonOf" />
                        <h5>Climate</h5>
                        <Slider v-model="site.heat" label="Heat" :min="-1" :max="1" :step="0.05" :format="signed" @update:model-value="place = null" />
                        <Slider v-model="site.wet" label="Rain" :min="-1" :max="1" :step="0.05" :format="signed" @update:model-value="place = null" />
                        <Slider v-model="site.season" label="Winter rain … summer rain" :min="-1" :max="1" :step="0.05" :format="signed" @update:model-value="place = null" />
                        <Slider v-model="site.light" label="Light under what's above" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <Slider v-model="site.wind" label="Wind" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <h5>Ground</h5>
                        <Slider v-model="site.fertility" label="Fertility" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <Slider v-model="site.sand" label="Sand" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <Slider v-model="site.clay" label="Clay" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <Slider v-model="site.stones" label="Stones" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <Slider v-model="site.depth" label="Soil depth" :min="0" :max="8" :step="1" :format="(v: number) => `${v} m`" @update:model-value="place = null" />
                        <Slider v-model="site.slope" label="Slope" :min="0" :max="2" :step="0.1" :format="signed" @update:model-value="place = null" />
                        <Slider v-model="site.upset" label="Fresh ground" :min="0" :max="1" :step="0.05" :format="share" @update:model-value="place = null" />
                        <Toggle v-model="site.bank" label="Beside water" @update:model-value="place = null" />
                    </template>
                    <template v-if="state">
                        <h5>What the site comes to</h5>
                        <dl>
                            <dt>Warmth</dt><dd>{{ share(state.strategy.warmth) }}</dd>
                            <dt>Water</dt><dd>{{ share(state.strategy.water) }}</dd>
                            <dt>Light</dt><dd>{{ share(state.strategy.light) }}</dd>
                            <dt>Vigour</dt><dd>{{ share(state.strategy.vigour) }}</dd>
                            <dt>Held back by</dt><dd>{{ LIMITS[state.strategy.limit] ?? state.strategy.limit }}</dd>
                        </dl>
                        <p>{{ STRATEGIES[state.strategy.strategy] ?? state.strategy.strategy }}.</p>
                        <p v-if="!state.traits.lives" class="dead">This lineage can't live here: {{ LIMITS[state.traits.why] ?? state.traits.why }}.</p>
                        <dl v-else>
                            <dt>Foliage</dt><dd>{{ state.traits.evergreen ? "evergreen" : state.traits.leafing < 0.5 ? "bare now" : "shed each year" }}{{ state.traits.flowering > 0 ? ", in flower" : "" }}</dd>
                            <dt>Grown in</dt><dd>{{ state.grain === 2 ? "half blocks" : state.grain === 4 ? "quarter blocks" : "sixteenths" }}</dd>
                            <dt>Blocks' worth</dt><dd>{{ worth(state.blocks) }}</dd>
                        </dl>
                    </template>
                </aside>
                <div class="player">
                    <Button size="sm" @click="plant++">Grow another</Button>
                    <Field v-model="world" class="world" label="World seed" :maxlength="24" mono />
                </div>
                <p v-if="problem" class="problem">{{ problem }}</p>
            </div>
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
/* nothing painted but the floats: the page's empty pixels are the scene's, and its mouse */
.scene { position: relative; width: 100%; height: 100%; min-height: 0; }
.facts { position: absolute; top: 12px; right: 12px; bottom: 12px; width: 300px; overflow-y: auto; display: flex; flex-direction: column; gap: 8px; padding: 14px; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.92); box-shadow: var(--raised); }
.facts h5 { margin: 8px 0 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.facts h5:first-child { margin-top: 0; }
.facts p { margin: 0; font-size: 13px; }
dl { display: grid; grid-template-columns: auto 1fr; gap: 3px 12px; margin: 0; font-size: 13px; }
dt { color: var(--ink-dim); }
dd { margin: 0; min-width: 0; text-align: right; }
.swatch { display: inline-block; width: 8px; height: 12px; margin-right: 2px; vertical-align: -1px; }
.swatch + span { margin-left: 6px; }
small { color: var(--ink-faint); font-size: 11px; }
.dead { color: var(--ink-faint); }
p.dead { color: var(--danger); }
.hint { color: var(--ink-faint); font-size: 12px; }
.set :deep(.value) { color: var(--accent); }
.player { position: absolute; left: 16px; bottom: 14px; display: flex; align-items: flex-end; gap: 12px; }
.world { width: 200px; }
.problem { position: absolute; left: 12px; top: 12px; margin: 0; padding: 6px 10px; color: var(--danger); font-size: 12px; background: rgb(21 24 27 / 0.92); }
</style>
