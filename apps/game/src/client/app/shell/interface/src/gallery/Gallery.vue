<script setup lang="ts">
import { ref } from "vue"
import { Tabs } from "../kit"
import { PAGES, type PageId } from "./pages"

/** Every component, in every state, without booting the game: `bun components`. */
const requested = new URLSearchParams(location.search).get("page")
const page = ref<PageId>(PAGES.find((item) => item.id === requested)?.id ?? PAGES[0].id)
</script>

<template>
    <div class="gallery">
        <header>
            <h1><span class="chev">»</span> Components</h1>
            <Tabs v-model="page" :tabs="PAGES" />
        </header>
        <main>
            <template v-for="p in PAGES" :key="p.id">
                <component :is="p.component" v-if="p.id === page" />
            </template>
        </main>
    </div>
</template>

<style scoped>
.gallery { height: 100%; display: flex; flex-direction: column; background: var(--stone-0); }
header {
    display: flex;
    align-items: center;
    gap: 28px;
    padding: 14px 24px;
    border-bottom: var(--b) solid var(--edge);
    background: var(--stone-1);
}
h1 { margin: 0; font: 400 18px var(--font-display); text-transform: uppercase; white-space: nowrap; }
.chev { color: var(--accent); }
header :deep(.tabs) { flex: 1; max-width: 720px; }
main { flex: 1; overflow: auto; padding: 28px; min-height: 0; }
</style>
