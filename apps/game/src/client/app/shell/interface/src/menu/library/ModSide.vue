<script setup lang="ts" generic="T extends string">
import { computed } from "vue"
import type { RegistryMod } from "../../bridge/index.ts"
import { FileTree, SideCard, SideNav, SideStats, type Icon } from "../../kit/index.ts"
import { ICONS } from "../../kit/icons.ts"
import { format } from "./format.ts"

/**
 * A mod page's side card, as the website's (registry/detail/sidebar.vue): what to look at first,
 * then its numbers, then the facts, then its files. The files are the latest version's: the
 * engine reads no other (bridge `registry.file`), so there's no version to choose.
 */
const props = defineProps<{ mod: RegistryMod; tabs: readonly { id: T; label: string; icon: Icon; count?: number | null }[]; file: string | null }>()
const tab = defineModel<T>("tab", { required: true })
const emit = defineEmits<{ file: [path: string] }>()
const stats = computed(() => [
    ...(props.mod.installs !== null ? [{ icon: ICONS.download, value: format.count(props.mod.installs), label: "installs" }] : []),
    { icon: ICONS.star, value: format.count(props.mod.stars), label: "stars" },
    { icon: ICONS.versions, value: String(props.mod.versions.length), label: "versions" },
])
const first = computed(() => props.mod.versions.at(-1) ?? null)
</script>

<template>
    <SideCard class="side">
        <SideNav v-model="tab" :items="tabs" />
        <SideStats :items="stats" />
        <dl class="meta">
            <div><dt>Latest</dt><dd>v{{ mod.latestVersion }}</dd></div>
            <div><dt>Updated</dt><dd>{{ format.date(mod.updatedAt) }}</dd></div>
            <div v-if="first"><dt>Published</dt><dd>{{ format.date(first.publishedAt) }}</dd></div>
            <div v-if="mod.versions[0]"><dt>Game API</dt><dd>{{ mod.versions[0].api }}</dd></div>
            <div v-if="mod.categories.length"><dt>{{ mod.categories.length === 1 ? "Category" : "Categories" }}</dt><dd class="filed">{{ mod.categories.join(", ") }}</dd></div>
        </dl>
        <section v-if="mod.files?.length" class="files flush">
            <div class="version"><span>Version</span><code>v{{ mod.latestVersion }} · latest</code></div>
            <FileTree :files="mod.files" :open="['server']" :model-value="file" @update:model-value="(p) => p && emit('file', p)" />
        </section>
    </SideCard>
</template>

<style scoped>
.meta { display: grid; gap: 0.45rem; font-size: 0.75rem; }
.meta div { display: flex; justify-content: space-between; gap: 1rem; }
dt { color: var(--ink-faint); }
dd { margin: 0; color: var(--ink-dim); text-align: right; }
.filed { color: var(--accent-hi); }
.files { padding: 1rem 0.5rem 0.75rem; }
.version { display: flex; gap: 0.6rem; align-items: center; margin: 0 0.5rem 0.75rem; color: var(--ink-faint); font: 0.6rem var(--font-display); text-transform: uppercase; }
.version code { flex: 1; display: flex; align-items: center; height: 2rem; padding: 0 0.5rem; border: 1px solid var(--glass-edge); color: var(--ink); font: 0.72rem var(--font); text-transform: none; }
</style>
