<script setup lang="ts">
import { onMounted } from "vue"
import { Turntable } from "../../kit/index.ts"
import { provideWiki, WikiNav } from "../../wiki/nav.ts"
import LibrarySection from "../../wiki/LibrarySection.vue"
import ModPage from "../../menu/library/ModPage.vue"
import type { LocalMod } from "../../bridge/index.ts"
import { mockFace, useMock } from "../mock.ts"

/** A mod of your own opened in the Library, the wiki's library as it is in a world, and the turntable alone. */
const mock = useMock()
provideWiki(WikiNav({ place: "game" }))
onMounted(() => mock.emit({ kind: "mode", mode: "game" }))
const mine: LocalMod = {
    name: "workshop", title: "Workshop", description: "A bench, bricks, a log and the first stone tools.", owner: "you", latestVersion: "0.1.0",
    stars: 0, updatedAt: new Date().toISOString(), type: "mod", categories: [], icon: null, banner: null, installs: null, installed: true,
    official: false, readme: "", deprecated: null, versions: [], media: null, files: null, dependencies: {},
    path: "/home/you/.terraverse/profiles/@you/mods/workshop",
}
const brick = { top: mockFace(["#5d5d64", "#7a7a82"], 1), bottom: mockFace(["#5d5d64", "#7a7a82"], 2), side: mockFace(["#4a4a50", "#6b6b72", "#7a7a82"], 3) }
</script>

<template>
    <div class="page">
        <section class="local"><ModPage :name="mine.name" :local="mine" back="Mods" /></section>
        <section class="library"><LibrarySection /></section>
        <section class="specimens">
            <Turntable :faces="brick" :size="120" />
            <Turntable :size="120" />
        </section>
    </div>
</template>

<style scoped>
.page { display: grid; gap: 16px; }
.local { border: var(--b) solid var(--edge); background: var(--stone-0); }
.library { height: 520px; border: var(--b) solid var(--edge); background: var(--stone-0); }
.specimens { display: flex; gap: 16px; }
.specimens > * { border: var(--b) solid var(--edge); background: var(--stone-1); }
</style>
