<script setup lang="ts">
import { computed } from "vue"
import { fileTree } from "./fileTree.ts"

/** A package's files as a tree; folders first, the chosen file lit. */
const props = defineProps<{ files: readonly { path: string; size: number }[]; root: string }>()
const chosen = defineModel<string | null>({ default: null })
const rows = computed(() => fileTree(props.files))
</script>

<template>
    <div class="file-tree" role="tree">
        <div class="root">▾ {{ root }}</div>
        <button v-for="row in rows" :key="row.path" type="button" role="treeitem" :disabled="row.folder"
            :class="{ folder: row.folder, on: chosen === row.path }" :style="{ '--depth': row.depth }"
            @click="chosen = row.path">
            <span class="glyph" :class="row.type">{{ row.folder ? "▸" : row.type }}</span>
            <span class="name">{{ row.name }}</span>
            <span v-if="!row.folder" class="size">{{ row.size }}</span>
        </button>
    </div>
</template>

<style scoped>
.file-tree { display: grid; gap: 1px; min-width: 0; font-size: 11px; }
.root { padding: 4px 6px; color: var(--ink); }
button { display: grid; grid-template-columns: 22px minmax(0, 1fr) auto; gap: 6px; align-items: center; padding: 3px 6px 3px calc(8px + var(--depth) * 14px); border: 0; background: none; color: var(--ink-dim); text-align: left; }
button:not(:disabled):hover { background: var(--stone-2); color: var(--ink); }
button.on { background: var(--accent-lo); color: var(--ink); }
button.folder { color: var(--ink-dim); cursor: default; }
.glyph { color: var(--ink-faint); font: 8px var(--font-display); text-transform: uppercase; }
.glyph.ts { color: var(--accent-hi); }
.glyph.md { color: #d7ad83; }
.glyph.json { color: #e0b33d; }
.glyph.png { color: #4a9b64; }
.name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.size { color: var(--ink-faint); font-size: 9px; }
</style>
