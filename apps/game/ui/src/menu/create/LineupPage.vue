<script setup lang="ts">
import { computed, ref } from "vue"
import { Button, ListItem, Section, Slider } from "../../kit/index.ts"
import { addMoon, addPartner, addPlanet, body, holdsStart, LIMITS, moonsOf, moveBody, partnerOf, planets, removeBody, STAR } from "../../system/model.ts"
import { useSystemPreview } from "../../system/preview.ts"
import BodyIcon from "../../worldgen/BodyIcon.vue"
import type { WorldDraftHandle } from "./draft.ts"
import { useLooks } from "./looks.ts"

/** The planets, moons and belts: which there are, and in what order from the star. */
const props = defineProps<{ draft: WorldDraftHandle }>()
const emit = defineEmits<{ reset: [] }>()
const looks = useLooks()
const preview = useSystemPreview()
const system = computed(() => props.draft.system)
const byOrbit = <T extends { orbit: number }>(list: T[]) => [...list].sort((a, z) => a.orbit - z.orbit)
const chosen = computed(() => system.value.selected === STAR ? null : body(system.value, system.value.selected))
const where = (orbit: number, moon: boolean) => moon ? `${orbit.toFixed(1)} radii` : `${orbit.toFixed(2)} AU`
const full = computed(() => system.value.bodies.length >= LIMITS.bodies)
const isPlanet = computed(() => chosen.value?.parent === null)
const canDouble = computed(() => !!chosen.value && isPlanet.value && !partnerOf(system.value, chosen.value.id) && preview.card(chosen.value.id)?.giant === false)

const belts = computed(() => props.draft.belts)
const openBelt = ref<string | null>(null)
const belt = computed(() => belts.value.find((b) => b.id === openBelt.value) ?? null)
function addBelt() {
    const n = system.value.made++
    const id = `belt-${n}`
    belts.value.push({ id: id, name: `Belt ${belts.value.length + 1}`, inner: 2.2, outer: 3.2, mass: -1.4 })
    openBelt.value = id
}
function removeBelt(id: string) {
    props.draft.belts = belts.value.filter((b) => b.id !== id)
    openBelt.value = null
}
const moons = (v: number) => `${(10 ** v).toPrecision(2)} Moons`
</script>

<template>
    <div class="lineup">
        <Section title="Planets and moons">
            <div class="list">
                <template v-for="planet in byOrbit(planets(system))" :key="planet.id">
                    <ListItem :on="system.selected === planet.id" @click="system.selected = planet.id">
                        <template #lead><BodyIcon kind="planet" :image="looks.image(planet.id)" /></template>
                        {{ planet.name }}
                        <template #trail>{{ where(planet.orbit, false) }}</template>
                    </ListItem>
                    <ListItem v-for="moon in byOrbit(moonsOf(system, planet.id))" :key="moon.id" class="moon"
                        :on="system.selected === moon.id" @click="system.selected = moon.id">
                        <template #lead><BodyIcon :kind="looks.image(moon.id) ? 'planet' : 'moon'" :image="looks.image(moon.id)" /></template>
                        {{ moon.name }}
                        <template #trail>{{ partnerOf(system, moon.id) ? 'double · ' : '' }}{{ where(moon.orbit, true) }}</template>
                    </ListItem>
                </template>
            </div>
        </Section>
        <div class="actions">
            <Button size="sm" :disabled="full" @click="addPlanet(system)">+ Planet</Button>
            <Button size="sm" :disabled="!isPlanet || full" @click="chosen && addMoon(system, chosen.id)">+ Moon</Button>
            <Button size="sm" :disabled="!canDouble || full" @click="chosen && addPartner(system, chosen.id)">+ Double</Button>
            <Button size="sm" :disabled="!chosen" @click="chosen && moveBody(system, chosen.id, -1)">Inward</Button>
            <Button size="sm" :disabled="!chosen" @click="chosen && moveBody(system, chosen.id, 1)">Outward</Button>
            <Button size="sm" variant="danger" :disabled="!chosen || holdsStart(system, chosen.id)" @click="chosen && removeBody(system, chosen.id)">Remove</Button>
        </div>
        <div v-if="draft.config.bodies" class="actions"><Button size="sm" @click="emit('reset')">Back to the seed's system</Button></div>
        <p class="copy">A double is a second planet as heavy as the first: they circle a point between them, and that point goes round the star.</p>

        <Section title="Belts" hint="not wired yet">
            <div v-if="belts.length" class="list">
                <ListItem v-for="item in belts" :key="item.id" :on="openBelt === item.id" @click="openBelt = item.id">
                    {{ item.name }}
                    <template #trail>{{ item.inner.toFixed(1) }}–{{ item.outer.toFixed(1) }} AU</template>
                </ListItem>
            </div>
            <template v-if="belt">
                <Slider v-model="belt.inner" label="Inner edge" :min="0.1" :max="belt.outer" :step="0.05" :format="(v) => `${v.toFixed(2)} AU`" />
                <Slider v-model="belt.outer" label="Outer edge" :min="belt.inner" :max="60" :step="0.05" :format="(v) => `${v.toFixed(2)} AU`" />
                <Slider v-model="belt.mass" label="Mass" :min="-3" :max="1" :step="0.05" :format="moons" />
            </template>
            <div class="actions">
                <Button size="sm" @click="addBelt">+ Belt</Button>
                <Button v-if="belt" size="sm" variant="danger" @click="removeBelt(belt.id)">Remove belt</Button>
            </div>
        </Section>
    </div>
</template>

<style scoped>
.lineup { display: flex; flex-direction: column; gap: 14px; min-width: 0; }
.list { display: flex; flex-direction: column; gap: 2px; }
.list :deep(.icon) { width: 22px; height: 22px; }
.list :deep(.list-item.lead) { grid-template-columns: 26px minmax(0, 1fr) auto; }
.moon { margin-left: 18px; width: calc(100% - 18px); }
.moon :deep(.icon) { width: 16px; height: 16px; }
.actions { display: flex; flex-wrap: wrap; gap: 6px; }
.copy { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
</style>
