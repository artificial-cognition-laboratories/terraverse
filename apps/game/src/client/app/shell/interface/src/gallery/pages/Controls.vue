<script setup lang="ts">
import { ref } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { Button, Choice, Field, IconButton, IconChoice, IconRail, IconTile, Notice, SearchBox, Section, Segmented, Slider, Tabs, Toggle } from "../../kit/index.ts"
import BodyIcon from "../../worldgen/BodyIcon.vue"
import { OVERLAY_ICONS } from "../../worldgen/overlayIcons.ts"
import Specimen from "../Specimen.vue"

const slider = ref(40)
const toggle = ref(true)
const text = ref("Pine Ridge")
const empty = ref("")
const size = ref(8192)
const tabRow = ref<"a" | "b" | "c">("a")
const tabCol = ref<"video" | "controls" | "audio">("video")
const choice = ref<"survival" | "creative" | null>("survival")
const iconChoice = ref<"continents" | "islands" | null>("continents")
const tile = ref("elevation")
const rail = ref("one")
</script>

<template>
    <div class="grid">
        <Specimen name="Button" note="variant × size, and disabled" wide>
            <div class="rows">
                <div v-for="size in (['lg', 'md', 'sm'] as const)" :key="size" class="row">
                    <Button variant="primary" :size="size">Primary</Button>
                    <Button :size="size">Plain</Button>
                    <Button variant="danger" :size="size">Danger</Button>
                    <Button :size="size" disabled>Disabled</Button>
                </div>
            </div>
        </Specimen>

        <Specimen name="Slider" :note="`value: ${slider}`">
            <div class="col"><Slider v-model="slider" label="Render distance" :min="20" :max="400" :step="10" :format="(v) => `${v} blocks`" /></div>
        </Specimen>

        <Specimen name="Toggle" :note="`value: ${toggle}`">
            <div class="col"><Toggle v-model="toggle" label="Shadows" /></div>
        </Specimen>

        <Specimen name="Field" note="filled, empty with placeholder, with action">
            <div class="col">
                <Field v-model="text" label="Name" />
                <Field v-model="empty" label="Seed" placeholder="random" mono>
                    <template #action><Button size="sm">Go</Button></template>
                </Field>
            </div>
        </Specimen>

        <Specimen name="SearchBox" note="compact search for panel headers">
            <SearchBox v-model="empty" label="Search settings" />
        </Specimen>

        <Specimen name="IconRail" note="group navigation; `end` items and the foot slot sit at the bottom" wide>
            <div style="height: 300px; width: 78px">
                <IconRail v-model="rail" label="Example groups" :items="[
                    { id: 'one', label: 'Mods', rows: ['.AAA.', 'AAAAA', '.AAA.', '..A..', '.....'], palette: { A: '#40b8ec' } },
                    { id: 'two', label: 'Worlds', rows: ['..A..', '.AAA.', 'AAAAA', '.AAA.', '..A..'], palette: { A: '#c9d4dc' } },
                    { id: 'three', label: 'Settings', end: true, rows: ['A.A.A', '.AAA.', 'AA.AA', '.AAA.', 'A.A.A'], palette: { A: '#c9d4dc' } },
                ]">
                    <template #foot><IconButton label="Account"><span>C</span></IconButton></template>
                </IconRail>
            </div>
        </Specimen>

        <Specimen name="Segmented" :note="`value: ${size}`">
            <div class="col"><Segmented v-model="size" :options="[2048, 4096, 8192, 16384].map((s) => ({ value: s, label: `${s / 1024}k` }))" /></div>
        </Specimen>

        <Specimen name="Tabs" note="row and column">
            <div class="col">
                <Tabs v-model="tabRow" :tabs="[{ id: 'a', label: 'World' }, { id: 'b', label: 'Terrain' }, { id: 'c', label: 'Climate' }]" />
                <Tabs v-model="tabCol" direction="column"
                    :tabs="[{ id: 'video', label: 'Video' }, { id: 'controls', label: 'Controls' }, { id: 'audio', label: 'Audio' }]" />
            </div>
        </Specimen>

        <Specimen name="Choice" :note="`value: ${choice ?? 'none (custom)'}`">
            <div class="col">
                <Choice v-model="choice" :options="[{ id: 'survival', label: 'Survival', note: 'gather, build and stay alive' }, { id: 'creative', label: 'Creative', note: 'every block, no danger' }]" />
                <Choice :model-value="null" disabled :options="[{ id: 'a', label: 'Disabled', note: 'not available here' }, { id: 'b', label: 'Options', note: 'greyed out' }]" />
            </div>
        </Specimen>

        <Specimen name="IconChoice" :note="`value: ${iconChoice ?? 'custom'}`">
            <IconChoice v-model="iconChoice" label="Planet shape" :columns="2" layout="stacked" :options="[
                { id: 'continents', label: 'Continents', rows: ['BBBBB', 'BLLBB', 'BLLLB', 'BBLBB', 'BBBBB'], palette: { B: '#17416a', L: '#6caa73' } },
                { id: 'islands', label: 'Islands', rows: ['BBBBB', 'BLBBB', 'BBBLB', 'BLBBB', 'BBBBB'], palette: { B: '#17416a', L: '#6caa73' } },
            ]" />
        </Specimen>

        <Specimen name="IconTile" note="hover for the label · on · disabled">
            <div class="row">
                <IconTile v-for="(icon, id) in OVERLAY_ICONS" :key="id" :label="id" :on="tile === id" :size="36" @click="tile = id">
                    <PixelIcon :rows="icon.rows" :palette="icon.palette" style="--u: 2px" />
                </IconTile>
                <IconTile label="Star"><BodyIcon kind="star" :color="[255, 190, 120]" /></IconTile>
                <IconTile label="Moon · concept"><BodyIcon kind="moon" /></IconTile>
            </div>
        </Specimen>

        <Specimen name="Notice" note="info · warn · danger">
            <div class="col">
                <Notice><strong>Preview only:</strong> this is a callout.</Notice>
                <Notice tone="warn"><strong>Careful:</strong> this changes the world.</Notice>
                <Notice tone="danger"><strong>Error:</strong> the world could not be saved.</Notice>
            </div>
        </Specimen>

        <Specimen name="Section">
            <div class="col">
                <Section title="Relief" hint="how the land rises">
                    <Slider v-model="slider" label="Mountains" :min="0" :max="100" :step="1" />
                </Section>
            </div>
        </Specimen>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(380px, 1fr)); gap: 28px; }
.rows { display: flex; flex-direction: column; gap: 14px; }
.row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
.col { display: flex; flex-direction: column; gap: 16px; width: 100%; }
</style>
