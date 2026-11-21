<script setup lang="ts">
import type { Node } from "~/registry/tree"

/** One folder or file in the source tree; a folder opens and closes, a file is picked. */
const props = defineProps<{ node: Node; depth: number; open: ReadonlySet<string>; active: string | null }>()
const emit = defineEmits<{ toggle: [path: string]; pick: [path: string] }>()

const ICONS: Record<string, string> = {
    ts: "vscode-icons:file-type-typescript-official",
    json: "vscode-icons:file-type-json",
    md: "vscode-icons:file-type-markdown",
    png: "vscode-icons:file-type-image",
}
const icon = computed(() => {
    if (props.node.kind === "folder") return props.open.has(props.node.path) ? "vscode-icons:default-folder-opened" : "vscode-icons:default-folder"
    if (props.node.name === "package.json") return "vscode-icons:file-type-npm"
    return ICONS[props.node.name.slice(props.node.name.lastIndexOf(".") + 1)] ?? "vscode-icons:default-file"
})
</script>

<template>
    <li>
        <button
            type="button"
            class="node"
            :class="{ active: node.kind === 'file' && node.path === active }"
            :style="{ paddingLeft: `${0.5 + depth * 0.9}rem` }"
            @click="node.kind === 'folder' ? emit('toggle', node.path) : emit('pick', node.path)"
        >
            <span class="caret" aria-hidden="true">{{ node.kind === "folder" ? (open.has(node.path) ? "▾" : "▸") : "" }}</span>
            <Icon :name="icon" class="icon" />
            <span class="name">{{ node.name }}</span>
        </button>
        <ul v-if="node.kind === 'folder' && open.has(node.path)">
            <registry-detail-tree-node
                v-for="child in node.children"
                :key="child.path"
                :node="child"
                :depth="depth + 1"
                :open="open"
                :active="active"
                @toggle="emit('toggle', $event)"
                @pick="emit('pick', $event)"
            />
        </ul>
    </li>
</template>

<style scoped lang="scss">
ul {
    margin: 0;
    padding: 0;
    list-style: none;
}

.node {
    display: flex;
    gap: 0.4rem;
    align-items: center;
    width: 100%;
    padding-top: 0.3rem;
    padding-bottom: 0.3rem;
    padding-right: 0.5rem;
    font-family: $font-mono;
    font-size: 0.74rem;
    text-align: left;
    color: $ink-dim;
    background: none;
    border: 1px solid transparent;
    cursor: pointer;

    &:hover {
        color: $ink;
        background: rgba($ink, 0.04);
    }

    &.active {
        color: $ink;
        background: rgba($primary, 0.3);
        border-color: rgba($primary-ink, 0.4);
    }
}

.caret {
    flex: none;
    width: 0.7rem;
    font-size: 0.6rem;
    color: $ink-faint;
}

.icon {
    flex: none;
    font-size: 0.95rem;
}

.name {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}
</style>
