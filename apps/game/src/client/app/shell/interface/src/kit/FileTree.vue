<script setup lang="ts">
import { computed, ref, watch } from "vue"
import PixelIcon from "../game/PixelIcon.vue"
import { fileTree, type TreeRow } from "./fileTree.ts"

/**
 * A package's files as the website's source tree (registry/detail/tree-node.vue): folders first,
 * each opening and closing on a click, a file picked and lit. Folders on the way to the chosen
 * file open by themselves.
 */
const props = withDefaults(defineProps<{ files: readonly { path: string; size: number }[]; open?: readonly string[] }>(), { open: () => [] })
const chosen = defineModel<string | null>({ default: null })
const rows = computed(() => fileTree(props.files))
const opened = ref(new Set<string>(props.open.map((p) => `${p}/`)))
const shown = computed(() => rows.value.filter((r) => ancestors(r.path).every((a) => opened.value.has(a))))
watch(chosen, (path) => { if (path) for (const a of ancestors(path)) opened.value.add(a) }, { immediate: true })

function click(row: TreeRow) {
    if (!row.folder) { chosen.value = row.path; return }
    const next = new Set(opened.value)
    if (!next.delete(row.path)) next.add(row.path)
    opened.value = next
}
/** The folders a path sits in, outermost first, each ending in "/". */
function ancestors(path: string): string[] {
    const parts = path.replace(/\/$/, "").split("/").slice(0, -1)
    return parts.map((_, i) => `${parts.slice(0, i + 1).join("/")}/`)
}
// a file's glyph: a folder, or a page in its type's colour
const FOLDER = { rows: ["GGG....", "GYYGGGG", "GYYYYYG", "GYYYYYG", "GYYYYYG", "GGGGGGG"], palette: { G: "#b08a2e", Y: "#e0b33d" } }
const COLOURS: Record<string, string> = { ts: "#63a3e9", js: "#e0b33d", json: "#e0603d", md: "#c9d4dc", png: "#5eba81" }
const page = (type: string) => ({ rows: ["GGGG...", "GLLLG..", "GLLLLG.", "GLLLLLG", "GLLLLLG", "GLLLLLG", "GGGGGGG"], palette: { G: COLOURS[type] ?? "#6f7f8a", L: "#0c121b" } })
</script>

<template>
    <ul class="file-tree" role="tree">
        <li v-for="row in shown" :key="row.path">
            <button type="button" class="node" :class="{ active: !row.folder && chosen === row.path }" role="treeitem"
                :style="{ paddingLeft: `${0.5 + row.depth * 0.9}rem` }" @click="click(row)">
                <span class="caret" aria-hidden="true">{{ row.folder ? (opened.has(row.path) ? "▾" : "▸") : "" }}</span>
                <PixelIcon v-bind="row.folder ? FOLDER : page(row.type)" style="--u: 2px" />
                <span class="name">{{ row.name }}</span>
            </button>
        </li>
    </ul>
</template>

<style scoped>
.file-tree { margin: 0; padding: 0; list-style: none; }
.node { display: flex; gap: 0.4rem; align-items: center; width: 100%; padding: 0.3rem 0.5rem; border: 1px solid transparent; background: none; color: var(--ink-dim); font: 0.74rem var(--font); text-align: left; }
.node:hover { color: var(--ink); background: rgb(231 237 244 / 0.04); }
.node.active { color: var(--ink); background: rgb(10 75 145 / 0.3); border-color: rgb(99 163 233 / 0.4); }
.caret { flex: none; width: 0.7rem; color: var(--ink-faint); font-size: 0.6rem; }
.node svg { flex: none; }
.name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
</style>
