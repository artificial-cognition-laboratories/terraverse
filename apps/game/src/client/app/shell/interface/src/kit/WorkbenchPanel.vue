<script setup lang="ts" generic="T extends string">
import { provide, toRef } from "vue"
import IconRail from "./IconRail.vue"
import Panel from "./Panel.vue"
import WorkbenchColumns from "./WorkbenchColumns.vue"
import { WORKBENCH_SEARCH } from "./workbench.ts"

/**
 * Every workspace screen (wiki, settings, community): one size, a rail of
 * groups, search at the top of its navigator. A screen fills either the list / content /
 * facts slots, or `workspace` when each group lays out its own columns.
 */
const props = defineProps<{
    title: string
    section: string
    groups: readonly { id: T; label: string; shortLabel?: string; rows: readonly string[]; palette: Record<string, string> }[]
    railLabel: string
    searchLabel?: string
    searchPlaceholder?: string
}>()
const group = defineModel<T>("group", { required: true })
const query = defineModel<string>("query", { default: "" })
provide(WORKBENCH_SEARCH, {
    query,
    label: toRef(props, "searchLabel"),
    placeholder: toRef(props, "searchPlaceholder"),
})
</script>

<template>
    <Panel :title="`${title} / ${section}`" class="workbench-panel">
        <template #actions>
            <slot name="headerActions" />
            <slot name="actions" />
        </template>
        <div class="workbench-frame">
            <IconRail v-model="group" :items="groups" :label="railLabel" />
            <div class="workbench-stage">
                <slot v-if="$slots.workspace" name="workspace" />
                <WorkbenchColumns v-else>
                    <template #list><slot name="list" /></template>
                    <template #content><slot name="content" /></template>
                    <template v-if="$slots.facts" #facts><slot name="facts" /></template>
                </WorkbenchColumns>
            </div>
        </div>
        <template v-if="$slots.footer" #footer><slot name="footer" /></template>
    </Panel>
</template>

<style scoped>
.workbench-panel { width: min(1340px, 97vw); height: min(840px, 94vh); }
.workbench-panel :deep(.body) { overflow: hidden; padding: 0; }
.workbench-frame { display: grid; grid-template-columns: 76px minmax(0, 1fr); width: 100%; height: 100%; min-height: 0; }
.workbench-stage { min-width: 0; min-height: 0; background: var(--stone-0); }
@media (max-width: 850px) {
    .workbench-frame { grid-template-columns: 66px minmax(0, 1fr); }
}
</style>
