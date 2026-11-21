<script setup lang="ts">
import type { RegistryMod } from "../../bridge/index.ts"
import { FileTree } from "../../kit/index.ts"
import { format } from "./format.ts"

/** Beside a README: the facts in numbers, and the latest version's files. */
defineProps<{ mod: RegistryMod }>()
const emit = defineEmits<{ file: [path: string] }>()
</script>

<template>
    <aside class="mod-aside">
        <dl>
            <div><dt>Latest</dt><dd>{{ mod.latestVersion }}</dd></div>
            <div v-if="mod.versions[0]"><dt>Mod API</dt><dd>v{{ mod.versions[0].api }}</dd></div>
            <div><dt>Versions</dt><dd>{{ mod.versions.length }}</dd></div>
            <div v-if="mod.versions[0]"><dt>Published</dt><dd>{{ format.ago(mod.versions[0].publishedAt) }}</dd></div>
            <div v-if="mod.categories.length"><dt>Categories</dt><dd>{{ mod.categories.join(", ") }}</dd></div>
        </dl>
        <section v-if="mod.files">
            <h4>Source</h4>
            <FileTree :files="mod.files" :root="mod.name" :model-value="null" @update:model-value="(p) => p && emit('file', p)" />
        </section>
    </aside>
</template>

<style scoped>
.mod-aside { display: grid; gap: 20px; min-width: 0; }
dl { display: grid; gap: 6px; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 10px; font-size: 11px; }
dt { color: var(--ink-faint); }
dd { margin: 0; color: var(--ink); text-align: right; overflow-wrap: anywhere; }
h4 { margin: 0 0 6px; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
</style>
