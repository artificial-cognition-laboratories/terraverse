<script setup lang="ts">
import { computed, onMounted } from "vue"
import { useBridge } from "../bridge/use.ts"
import { Button, WorkbenchPanel } from "../kit/index.ts"
import { AIR_DEMOS } from "./air.ts"
import { LAB_DEMOS } from "./demos.ts"
import DemosSection from "./DemosSection.vue"
import FloraSection from "./FloraSection.vue"
import FluidsSection from "./FluidsSection.vue"
import MatterSection from "./MatterSection.vue"
import MovementSection from "./MovementSection.vue"
import WorldsSection from "./WorldsSection.vue"
import { provideWiki, WikiNav, type Section } from "./nav.ts"
import ReactionsSection from "./ReactionsSection.vue"

/**
 * The wiki: what everything is made of and how it behaves, shown rather
 * than told. An icon rail picks the section; each has its list, a live
 * view, and the facts. The same page in the menu and in game (key G, where
 * the world's rules allow it).
 */
const props = defineProps<{ place: "menu" | "game" }>()
defineEmits<{ close: [] }>()
const bridge = useBridge()
const wiki = WikiNav({ place: props.place })
provideWiki(wiki)

onMounted(async () => {
    const t = await bridge.send("matter.list", {})
    wiki.table.value = { ...t, bySymbol: new Map(t.elements.map((e) => [e.symbol, e])), byId: new Map(t.substances.map((s) => [s.id, s])) }
})

// 9×9 pictograms for the rail
const RAIL: { id: Section; label: string; shortLabel?: string; rows: string[]; palette: Record<string, string> }[] = [
    { id: "matter", label: "Matter", palette: { O: "#c9d4dc", N: "#ff8a3d" },
        rows: ["...O.O...", "..O...O..", ".O.....O.", "O...N...O", "O..NNN..O", "O...N...O", ".O.....O.", "..O...O..", "...O.O..."] },
    { id: "reactions", label: "Reactions", shortLabel: "React", palette: { F: "#ffb040", R: "#e0603d", W: "#8a5a32" },
        rows: ["....F....", "...FRF...", "..FRRF...", "..FRRRF..", ".FRRRRF..", ".FRRRRRF.", "..FRRRF..", ".WWWWWWW.", "..W...W.."] },
    { id: "heat", label: "Heat & fire", shortLabel: "Heat", palette: { F: "#ffb040", R: "#e0603d", S: "#c9d4dc" },
        rows: ["..S...F..", "..S..FF..", "..S.FRF..", "..S.FRRF.", "..S.FRRF.", ".SSS.FF..", ".SSS.....", ".SSS.....", "..S......"] },
    { id: "fluids", label: "Fluids & ground", shortLabel: "Flow", palette: { W: "#4aa3df", L: "#a7d8f0", S: "#d8c58a" },
        rows: ["....L....", "....W....", "...WWW...", "...WLW...", "..WWWWW..", "..WLLWW..", "...WWW...", ".SSSSSSS.", "SSSSSSSSS"] },
    { id: "air", label: "Air & pressure", shortLabel: "Air", palette: { C: "#c9d4dc", F: "#ffb040" },
        rows: ["..CCC....", ".C...C.CC", "......C..", "CCCCCC...", ".........", "CCCCCCCC.", "........C", "...F...C.", "..FFF.CC."] },
    { id: "worlds", label: "Worlds", palette: { G: "#5f7a45", B: "#3f76e4", S: "#c9b27c", R: "#7c7c78" },
        rows: [".....R...", "....RRR..", "...RRRRR.", "..GGRBRG.", ".GGGGBGG.", "GGGGBBGGG", "GGSBBSGGG", "SSBBBSSSS", "BBBBBBBBB"] },
    { id: "flora", label: "Flora", palette: { L: "#4f8a35", D: "#2f6a2a", W: "#6b5a48", G: "#5f7a45" },
        rows: ["...LLL...", "..LLDLL..", ".LLDLLLL.", ".LLLLDLL.", "..LDLLL..", "...LWL...", "....W....", "....W....", ".GGGWGGG."] },
    { id: "movement", label: "Movement", shortLabel: "Move", palette: { S: "#d8a07a", B: "#0094d2", T: "#8a93ad" },
        rows: ["....SS...", "....SS...", "...BBB.B.", "..B.BBB..", ".B..BB...", "....TT...", "...T..T..", "..T...T..", ".T.....T."] },
    { id: "vehicles", label: "Vehicles", shortLabel: "Drive", palette: { B: "#0094d2", W: "#c9d4dc", D: "#3a3f55" },
        rows: [".........", "...BBBB..", "..BWWWB..", ".BBBBBBBB", ".BBBBBBBB", ".DDBBBDDB", ".DWDBDWD.", "..D...D..", "........."] },
]
const section = computed({
    get: () => wiki.state.section,
    set: (id: Section) => wiki.go(id),
})
const sectionLabel = computed(() => RAIL.find((item) => item.id === section.value)?.label ?? "Wiki")
const query = wiki.search
</script>

<template>
    <WorkbenchPanel v-model:group="section" v-model:query="query" class="wiki"
        title="Wiki" :section="sectionLabel" :groups="RAIL" rail-label="Wiki sections"
        :search-label="`Search ${sectionLabel.toLowerCase()}`"
        :search-placeholder="`Search ${sectionLabel.toLowerCase()}…`">
        <template #actions><Button size="sm" @click="$emit('close')">{{ place === "menu" ? "Back" : "Close" }}</Button></template>
        <template #workspace>
            <div v-if="wiki.table.value" class="section">
                <MatterSection v-if="wiki.state.section === 'matter'" />
                <ReactionsSection v-else-if="wiki.state.section === 'reactions'" />
                <DemosSection v-else-if="wiki.state.section === 'heat'" key="heat" section="heat" :demos="LAB_DEMOS" />
                <DemosSection v-else-if="wiki.state.section === 'air'" key="air" section="air" :demos="AIR_DEMOS" />
                <WorldsSection v-else-if="wiki.state.section === 'worlds'" />
                <FloraSection v-else-if="wiki.state.section === 'flora'" />
                <FluidsSection v-else-if="wiki.state.section === 'fluids'" />
                <MovementSection v-else-if="wiki.state.section === 'movement' || wiki.state.section === 'vehicles'" :key="wiki.state.section" :kind="wiki.state.section" />
            </div>
            <p v-else class="loading">reading the tables…</p>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
/* the middle is a hole: the scene's drawn natively under the page (src/shell/scene.h). What's left paints its own. */
.wiki { background: none; }
.wiki :deep(.body), .wiki :deep(.workbench-stage) { background: none; }
.section { min-width: 0; min-height: 0; height: 100%; }
.loading { color: var(--ink-faint); }
</style>
