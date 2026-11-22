<script setup lang="ts">
import { computed, inject, useSlots } from "vue"
import SearchBox from "./SearchBox.vue"
import { WORKBENCH_SEARCH } from "./workbench.ts"

/**
 * The same list → content → facts hierarchy on every workspace screen.
 * The facts column shows when there's a facts slot; the columns own the
 * type inside them, so every screen's facts read alike.
 */
const props = withDefaults(defineProps<{ listWidth?: number; factsWidth?: number; contentWidth?: number; /** The content is a native scene under the page: nothing painted, nothing padded. */ open?: boolean }>(), {
    listWidth: 260,
    factsWidth: 380,
    contentWidth: 600,
})
const slots = useSlots()
const columns = computed(() => `${props.listWidth}px minmax(0, 1fr)${slots.facts ? ` ${props.factsWidth}px` : ""}`)
const search = inject(WORKBENCH_SEARCH, null)
const searchLabel = computed(() => search?.label.value)
const searchPlaceholder = computed(() => search?.placeholder.value)
const searchQuery = computed({
    get: () => search?.query.value ?? "",
    set: (value: string) => { if (search) search.query.value = value },
})
</script>

<template>
    <div class="workbench-columns" :style="{ gridTemplateColumns: columns }">
        <nav class="workbench-list">
            <div v-if="searchLabel || $slots.listTools" class="workbench-list-tools">
                <SearchBox v-if="searchLabel" v-model="searchQuery" :label="searchLabel" :placeholder="searchPlaceholder" />
                <slot name="listTools" />
            </div>
            <div class="workbench-list-scroll"><slot name="list" /></div>
        </nav>
        <main class="workbench-content" :class="{ open }">
            <div class="workbench-content-inner" :style="open ? undefined : { maxWidth: `${contentWidth}px` }"><slot name="content" /></div>
        </main>
        <aside v-if="$slots.facts" class="workbench-facts"><slot name="facts" /></aside>
    </div>
</template>

<style scoped>
/* the library's look (kit/Screen, SideCard): the night washed over what's behind, hairline edges */
.workbench-columns { display: grid; width: 100%; height: 100%; min-width: 0; min-height: 0; }
.workbench-list, .workbench-content, .workbench-facts { min-width: 0; min-height: 0; }
.workbench-list { display: flex; flex-direction: column; overflow: hidden; border-right: 1px solid var(--glass-edge); background: var(--side); }
.workbench-list-tools { flex: none; padding: 0.75rem; border-bottom: 1px solid var(--glass-edge); }
.workbench-list-tools :deep(.search-box) { width: 100%; }
.workbench-list-scroll { min-height: 0; padding: 0.75rem 0.5rem 1.25rem; overflow-y: auto; }
.workbench-content, .workbench-facts { overflow-y: auto; }
.workbench-content, .workbench-facts { display: flex; flex-direction: column; gap: 0.75rem; padding: 1.25rem 1.5rem; background: var(--wash); }
.workbench-content-inner { display: flex; flex-direction: column; gap: 0.75rem; width: 100%; min-width: 0; min-height: 100%; margin-inline: auto; }
.workbench-content.open { padding: 0; background: none; overflow: hidden; }
.workbench-content.open > .workbench-content-inner { height: 100%; }
.workbench-facts { border-left: 1px solid var(--glass-edge); }
.workbench-facts :deep(h4) { margin: 0; font: 400 1.1rem var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.workbench-facts :deep(h5) { margin: 0.4rem 0 0; color: var(--ink-faint); font: 0.62rem var(--font-display); letter-spacing: 0.08em; text-transform: uppercase; }
.workbench-columns :deep(.dim) { margin: 0; color: var(--ink-faint); font-size: 0.72rem; }
.workbench-columns :deep(.copy) { margin: 0; color: var(--ink-dim); font: 0.85rem/1.55 var(--font-sans); }
.workbench-facts :deep(dl) { display: grid; gap: 0.45rem; margin: 0; }
.workbench-facts :deep(dl div) { display: flex; justify-content: space-between; gap: 1rem; font-size: 0.75rem; }
.workbench-facts :deep(dt) { color: var(--ink-faint); }
.workbench-facts :deep(dd) { margin: 0; color: var(--ink-dim); text-align: right; }
.workbench-columns :deep(.link) { padding: 0; border: 0; background: none; color: var(--accent-hi); font-size: 0.78rem; text-align: left; cursor: pointer; }
.workbench-columns :deep(.link:hover) { color: var(--ink); }
.workbench-facts :deep(.links) { display: flex; flex-wrap: wrap; gap: 0.25rem 0.75rem; }
@media (max-width: 900px) {
    .workbench-columns { grid-template-columns: 200px minmax(0, 1fr) !important; }
    .workbench-facts { display: none; }
}
@media (max-width: 560px) {
    .workbench-columns { grid-template-columns: 140px minmax(0, 1fr) !important; }
    .workbench-content { padding: 14px; }
}
</style>
