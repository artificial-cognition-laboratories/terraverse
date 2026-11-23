<script setup lang="ts">
import { computed } from "vue"
import { Button, IconTile, Notice } from "@game/kit/index.ts"
import PixelIcon from "@game/game/PixelIcon.vue"
import Backdrop from "../parts/Backdrop.vue"
import PlayButton from "../parts/PlayButton.vue"
import VersionPicker from "../parts/VersionPicker.vue"
import { useStore } from "../use.ts"

/**
 * The landing: the planet rising centred at the foot, and in the sky above it the word with,
 * under it, which version and Play (the profile at the top right, App.vue). What's waiting
 * (offline, a failed install) stands at the top right, under the profile.
 */
const DOCS = "https://terraverse.arclabs.it/docs"
const REPO = "https://github.com/artificial-cognition-laboratories/terraverse"
// the game menu's open book (interface menu/MainMenu.vue WIKI_ICON)
const DOCS_ICON = {
    rows: [".........", "BBBB.BBBB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BBBBBBBBB", "....B...."],
    palette: { B: "#c9d4dc", P: "#f0ead8", L: "#8a8478" },
} as const
const store = useStore()
const { state } = store

const failure = computed(() => {
    const target = store.target.value
    const standing = target ? store.standing(target.id) : null
    return standing?.state === "failed" ? standing.reason : null
})
</script>

<template>
    <div class="home">
        <Backdrop />
        <div class="shade" aria-hidden="true" />

        <div class="top">
            <h1>Terraverse</h1>

            <!-- under the word: which version, and Play -->
            <div class="foot">
                <VersionPicker />
                <div class="play">
                    <PlayButton v-if="store.target.value" :version="store.target.value" />
                    <div v-else-if="state.manifest" class="none">
                        <span>No versions yet.</span>
                        <Button size="sm" @click="store.refreshManifest()">Check again</Button>
                    </div>
                </div>
            </div>
        </div>

        <!-- the game menu's corner tiles (interface menu/MainMenu.vue .tools): the docs, the source -->
        <nav class="tools" aria-label="Links">
            <IconTile label="The docs" side="left" @click="store.openUrl(DOCS)">
                <PixelIcon :rows="DOCS_ICON.rows" :palette="DOCS_ICON.palette" style="--u: 3px" />
            </IconTile>
            <IconTile label="Terraverse on GitHub" side="left" @click="store.openUrl(REPO)">
                <svg viewBox="0 0 16 16" width="23" height="23" aria-hidden="true">
                    <path fill="currentColor" d="M8 1a7 7 0 0 0-2.21 13.64c.35.06.48-.15.48-.34v-1.19c-1.96.42-2.37-.83-2.37-.83-.32-.81-.78-1.03-.78-1.03-.64-.44.05-.43.05-.43.71.05 1.08.73 1.08.73.63 1.07 1.65.76 2.05.58.06-.46.25-.76.45-.93-1.56-.18-3.2-.78-3.2-3.48 0-.77.27-1.4.72-1.9-.07-.18-.31-.9.07-1.87 0 0 .59-.19 1.93.73A6.7 6.7 0 0 1 8 4.45c.59 0 1.18.08 1.73.23 1.34-.92 1.93-.73 1.93-.73.38.97.14 1.69.07 1.87.45.5.72 1.13.72 1.9 0 2.71-1.64 3.3-3.2 3.47.25.22.48.64.48 1.29v1.82c0 .19.13.4.48.34A7 7 0 0 0 8 1Z" />
                </svg>
            </IconTile>
        </nav>

        <div class="dock">
            <Notice v-if="state.offline" tone="warn">{{ state.offline }} Showing what was known when last online.</Notice>
            <Notice v-if="failure" tone="danger">{{ failure }}</Notice>
        </div>
    </div>
</template>

<style scoped>
.home { position: relative; height: 100%; overflow: hidden; container-type: size; }
.shade { position: absolute; inset: 0; pointer-events: none; background: none; }

/* the word and the group, one column centred in the sky above the planet's limb */
.top { position: absolute; top: 22%; left: 0; right: 0; bottom: 32%; display: flex; flex-direction: column; justify-content: center; align-items: center;
    gap: clamp(1rem, 4cqh, 1.75rem); padding: 24px; }
h1 { margin: 0; font: 400 clamp(1.4rem, min(6.3cqw, 9.8cqh), 4.5rem) / 0.9 var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; color: var(--ink);
    text-shadow: 0 0.08em 0 var(--edge), 0 0 6px var(--night-0), 0 0 12px var(--night-0); }

/* picker and Play joined, one edge round both */
.foot { display: grid; grid-template-columns: auto 190px; max-width: 100%; box-shadow: 0 10px 30px rgb(0 0 0 / 0.45); }
.foot > :first-child :deep(.current) { border-right: 0; background: var(--glass-strong); }
.none { display: flex; align-items: center; justify-content: space-between; gap: 12px; padding-left: 14px; height: 3.1rem;
    border: 1px solid var(--glass-edge); background: var(--glass); font: 0.75rem var(--font-sans); color: var(--ink-dim); }

.tools { position: absolute; right: 20px; bottom: 20px; display: flex; gap: 10px; }

/* what's waiting, under the profile at the top right */
.dock { position: absolute; right: 20px; top: 84px; display: flex; flex-direction: column; align-items: flex-end; gap: 8px; max-width: min(30rem, calc(100% - 40px)); }
.dock :deep(.notice) { max-width: 30rem; }

/* narrow: the group takes the width */
/* small: picker over Play, one column; the corner tiles come down a size, as the rail's icons do */
@container (max-width: 600px) {
    .foot { grid-template-columns: minmax(0, 1fr); grid-template-rows: auto auto; width: min(16rem, 100%); }
    .foot > :first-child :deep(.current) { border-right: 1px solid var(--glass-edge); border-bottom: 0; }
}
@container (max-width: 600px) or (max-height: 420px) {
    .tools { right: 12px; bottom: 12px; gap: 6px; }
    .tools :deep(.tile) { width: 36px; height: 36px; }
    .tools :deep(.tile svg) { transform: scale(0.82); }
}
</style>
