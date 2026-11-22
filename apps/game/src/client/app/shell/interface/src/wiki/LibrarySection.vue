<script setup lang="ts">
import { onMounted, ref, shallowRef } from "vue"
import type { CreativeItem } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { Notice } from "../kit/index.ts"
import LibraryBrowser from "./LibraryBrowser.vue"
import { useWiki } from "./nav.ts"

/**
 * The wiki's library: every block and item this world has, as the engine registered them, so
 * a mod's art shows as the game draws it. Only in a world: the menu has none to list.
 */
const bridge = useBridge()
const wiki = useWiki()
const catalog = shallowRef<CreativeItem[] | null>(null)
const problem = ref<string | null>(null)

onMounted(async () => {
    if (wiki.place !== "game") return
    try {
        catalog.value = await bridge.send("creative.list", {})
    } catch (err) {
        problem.value = err instanceof Error ? err.message : String(err)
    }
})
</script>

<template>
    <div v-if="wiki.place !== 'game'" class="away">
        <Notice>The library shows a world's blocks and items, as its mods declared them: open the wiki in a world (G), or a mod's Contents in the Library.</Notice>
    </div>
    <LibraryBrowser v-else :catalog="catalog" :problem="problem" :query="wiki.search.value" />
</template>

<style scoped>
.away { display: grid; place-items: center; height: 100%; background: var(--stone-0); }
</style>
