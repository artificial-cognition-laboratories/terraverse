<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { CreativeItem } from "../bridge/index.ts"
import { ListEmpty, ListGroup, ListItem, Notice, WorkbenchColumns } from "../kit/index.ts"
import LibraryViewer from "./LibraryViewer.vue"

/**
 * Blocks and items by the mod that declared them: pick one and it fills the stage. Whoever
 * holds the catalogue (the wiki, from the engine; a mod's page, from terra) passes it in,
 * and may pass a newer one at any time: what's picked stays picked while it's still there.
 */
const props = defineProps<{
    catalog: readonly CreativeItem[] | null
    /** Shown over the stage: why the catalogue isn't what it should be. */
    problem?: string | null
    /** Text to narrow the list by, in a name or description. */
    query?: string
}>()

const chosen = ref<string | null>(null)
watch(() => props.catalog, c => {
    if (c && !c.some(i => i.item === chosen.value)) chosen.value = c[0]?.item ?? null
}, { immediate: true })

/** "workshop:stone_axe" → "workshop": who declared it. */
const modOf = (name: string) => name.slice(0, name.indexOf(":"))

const groups = computed(() => {
    const q = (props.query ?? "").trim().toLowerCase()
    const shown = (props.catalog ?? []).filter(i => !q || `${i.item} ${i.description}`.toLowerCase().includes(q))
    const byMod = new Map<string, CreativeItem[]>()
    for (const i of shown) byMod.set(modOf(i.item), [...(byMod.get(modOf(i.item)) ?? []), i])
    // the engine's own last: what a mod adds is what you're likely looking for
    return [...byMod.entries()].sort(([a], [b]) => Number(a === "core") - Number(b === "core") || a.localeCompare(b))
})
const picked = computed(() => props.catalog?.find(i => i.item === chosen.value) ?? null)
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <ListGroup v-for="[mod, items] in groups" :key="mod" :title="mod">
                <ListItem v-for="i in items" :key="i.item" :on="chosen === i.item" @click="chosen = i.item">
                    <template #lead><img v-if="i.icon" class="icon" :src="i.icon" alt=""></template>{{ i.description }}
                </ListItem>
            </ListGroup>
            <ListEmpty v-if="catalog && !groups.length">No matches.</ListEmpty>
        </template>

        <template #content>
            <div class="stage">
                <LibraryViewer v-if="picked" :item="picked" />
                <Notice v-if="problem" class="problem" tone="danger">{{ problem }}</Notice>
            </div>
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
/* open columns paint nothing: the stage paints itself, and the viewer takes all of it */
.stage { position: relative; display: grid; place-items: center; height: 100%; min-height: 0; overflow: hidden; background: var(--wash); }
.problem { position: absolute; left: 16px; right: 16px; bottom: 56px; white-space: pre-wrap; font-family: var(--font-mono, monospace); }
.icon { width: 20px; height: 20px; image-rendering: pixelated; }
</style>
