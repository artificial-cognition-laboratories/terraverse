<script setup lang="ts">
import { computed, ref } from "vue"
import { useBridge } from "../bridge/use"
import { Button, WorkbenchPanel } from "../kit"
import type { Screen } from "./screens"

defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()
type Category = "music" | "art" | "fonts" | "technology"
const category = ref<Category>("music")
const CATEGORIES = [
    { id: "music", label: "Music", rows: ["...AAA...", "..AAAAA..", ".AA.AAA..", ".AA.AAA..", ".AA.AAA..", ".AA.AAA..", ".AA.AAA..", ".AAAAAAA.", "..AAAAA.."], palette: { A: "#40b8ec" } },
    { id: "art", label: "Artwork", shortLabel: "Art", rows: ["..AAAAA..", ".ABBBBBA.", "ABCCCCCBA", "ABCCCCCBA", "ABCDDDCBA", "ABCCCCCBA", "ABCCCCCBA", ".ABBBBBA.", "..AAAAA.."], palette: { A: "#c9d4dc", B: "#40b8ec", C: "#6caa73", D: "#d9ae69" } },
    { id: "fonts", label: "Type", rows: ["AAAAAAAAA", "...AAA...", "...AAA...", "...AAA...", "...AAA...", "...AAA...", "...AAA...", "...AAA...", "..AAAAA.."], palette: { A: "#c9d4dc" } },
    { id: "technology", label: "Engine", rows: ["...AAA...", ".AAAAAAA.", "AA..A..AA", "AAAAAAAAA", "AA..A..AA", "AAAAAAAAA", "AA..A..AA", ".AAAAAAA.", "...AAA..."], palette: { A: "#c9d4dc" } },
] as const
const title = computed(() => CATEGORIES.find((item) => item.id === category.value)!.label)

type Credit = { title: string; by: string; detail: string; source?: string; sourceLabel?: string }
const CREDITS: Record<Category, { intro: string; entries: Credit[] }> = {
    music: {
        intro: "The sounds that give Terraverse its sense of place.",
        entries: [
            { title: "If Gaia Was One Of Us Part Four", by: "Denovaire", detail: "Local menu preview · release rights pending", source: "https://uppbeat.io/contributors/denovaire", sourceLabel: "Artist" },
            { title: "The Farwatch Plains", by: "Ian Aisling", detail: "Track on file · not in the game yet", source: "https://uppbeat.io/contributors/ian-aisling", sourceLabel: "Artist" },
            { title: "Perspectives", by: "Kevin MacLeod", detail: "Track on file · not in the game yet", source: "https://uppbeat.io/music/tracks/kevin-macleod/perspectives", sourceLabel: "Track" },
        ],
    },
    art: {
        intro: "The materials and original art that shape the world.",
        entries: [
            { title: "Good Vibes", by: "Acaitart", detail: "Block textures · CC BY 4.0. Selected art is retinted and adapted for Terraverse.", source: "https://github.com/Phyronnaz/VoxelAssets/tree/f1bb6fb20e36a47b8dacbdf23c4a10b994b6f224/GoodVibes", sourceLabel: "Source" },
            { title: "Original game art", by: "Terraverse", detail: "Interface pixel icons, weather particles, tools and effects made for the game." },
        ],
    },
    fonts: {
        intro: "The typefaces used throughout the interface.",
        entries: [
            { title: "Silkscreen", by: "Jason Kottke", detail: "Display lettering · SIL Open Font License", source: "https://fonts.google.com/specimen/Silkscreen", sourceLabel: "Font" },
            { title: "JetBrains Mono", by: "JetBrains", detail: "Interface text · SIL Open Font License", source: "https://www.jetbrains.com/lp/mono/", sourceLabel: "Font" },
        ],
    },
    technology: {
        intro: "The open-source foundations beneath the game.",
        entries: [
            { title: "Luanti", by: "The Luanti contributors", detail: "Voxel engine · LGPL 2.1 or later", source: "https://www.luanti.org/", sourceLabel: "Project" },
            { title: "Chromium Embedded Framework", by: "The CEF contributors", detail: "Embedded browser that draws Terraverse's interface", source: "https://bitbucket.org/chromiumembedded/cef", sourceLabel: "Project" },
        ],
    },
}
</script>

<template>
    <WorkbenchPanel v-model:group="category" class="attributions" title="Attributions" :section="title"
        :groups="CATEGORIES" rail-label="Attribution categories">
        <template #actions><Button size="sm" @click="$emit('go', 'home')">Back</Button></template>
        <template #workspace>
            <div class="credits">
                <header class="heading">
                    <span>WITH THANKS TO</span>
                    <h2>{{ title }}</h2>
                    <p>{{ CREDITS[category].intro }}</p>
                </header>
                <div class="entries">
                    <article v-for="entry in CREDITS[category].entries" :key="entry.title" class="entry">
                        <div class="credit-copy">
                            <h3>{{ entry.title }}</h3>
                            <p class="by">{{ entry.by }}</p>
                            <p class="detail">{{ entry.detail }}</p>
                        </div>
                        <button v-if="entry.source" type="button" class="source" @click="bridge.send('open', { url: entry.source })">
                            {{ entry.sourceLabel }} »
                        </button>
                    </article>
                </div>
                <p class="full-credits">Full asset sources and licence details are recorded in <strong>assets/CREDITS.md</strong>.</p>
            </div>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
.attributions { width: min(960px, 94vw); height: min(660px, 84vh); }
.credits { height: 100%; min-width: 0; overflow-y: auto; padding: 28px clamp(24px, 4vw, 56px); }
.heading { padding-bottom: 24px; border-bottom: 1px solid var(--stone-4); }
.heading span { color: var(--accent-hi); font: 10px var(--font-display); }
.heading h2 { margin: 9px 0 10px; font: 22px var(--font-display); text-transform: uppercase; }
.heading p { margin: 0; color: var(--ink-dim); font-size: 12px; }
.entries { display: grid; }
.entry { display: flex; justify-content: space-between; gap: 24px; padding: 22px 0; border-bottom: 1px solid var(--stone-3); }
.credit-copy { min-width: 0; }
.entry h3 { margin: 0; font: 13px var(--font-display); overflow-wrap: anywhere; }
.entry p { margin: 5px 0 0; font-size: 12px; }
.by { color: var(--accent-hi); }
.detail { color: var(--ink-dim); line-height: 1.5; }
.source { align-self: flex-start; flex: none; padding: 0; border: 0; background: none; color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; }
.source:hover, .source:focus-visible { color: var(--ink); text-decoration: underline; }
.full-credits { margin: 26px 0 0; color: var(--ink-faint); font-size: 11px; }
.full-credits strong { color: var(--ink-dim); font-weight: 400; }
@media (max-width: 650px) { .credits { padding: 22px; } .entry { gap: 12px; } }
</style>
