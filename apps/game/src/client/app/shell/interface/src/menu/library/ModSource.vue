<script setup lang="ts">
import { watch } from "vue"
import type { RegistryMod } from "../../bridge/index.ts"
import { FileTree, Markdown, Notice, Spinner } from "../../kit/index.ts"
import { useRegistry } from "./registry.ts"
import { Request } from "./request.ts"

/** The latest version's files: the tree at the left, the chosen file at the right, as text. */
const props = defineProps<{ mod: RegistryMod }>()
const file = defineModel<string | null>({ required: true })
const registry = useRegistry()
const text = Request<string, string>((path) => registry.file(props.mod.name, path))
const IMAGE = /\.(png|jpe?g|gif|webp)$/i
watch(file, (path) => { if (path && !IMAGE.test(path)) text.run(path) }, { immediate: true })
watch(() => props.mod.files, (files) => { if (!file.value && files?.length) file.value = (files.find((f) => /readme/i.test(f.path)) ?? files[0])?.path ?? null }, { immediate: true })
</script>

<template>
    <div class="source">
        <FileTree v-model="file" :files="mod.files ?? []" :root="mod.name" class="tree" />
        <div class="view">
            <div v-if="file" class="path">{{ file }}</div>
            <p v-if="file && IMAGE.test(file)" class="dim">Images show in the README.</p>
            <Notice v-else-if="text.problem.value" tone="danger">{{ text.problem.value }}</Notice>
            <div v-else-if="text.busy.value" class="dim"><Spinner /></div>
            <Markdown v-else-if="file && /\.md$/i.test(file) && text.data.value !== null" :source="text.data.value" />
            <pre v-else-if="text.data.value !== null"><code>{{ text.data.value }}</code></pre>
        </div>
    </div>
</template>

<style scoped>
.source { display: grid; grid-template-columns: 240px minmax(0, 1fr); gap: 18px; align-items: start; }
.tree { padding: 8px 0; border: var(--b) solid var(--edge); background: var(--stone-1); }
.view { display: grid; gap: 10px; min-width: 0; }
.path { color: var(--ink-faint); font-size: 11px; }
pre { margin: 0; padding: 14px; overflow-x: auto; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); color: var(--ink); font-size: 11px; line-height: 1.55; user-select: text; tab-size: 4; }
.dim { color: var(--ink-faint); font-size: 12px; }
</style>
