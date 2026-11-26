<script setup lang="ts">
import type { Mod } from "~/registry"
import type { Node } from "~/registry/tree"

/** A version's files: the version to look at, then its tree; the open file lit. */
defineProps<{ mod: Mod; version: string; tree: readonly Node[]; open: ReadonlySet<string>; file: string | null }>()
const emit = defineEmits<{ version: [version: string]; toggle: [path: string]; pick: [path: string] }>()

function chose(event: Event) {
    if (event.target instanceof HTMLSelectElement) emit("version", event.target.value)
}
</script>

<template>
    <section class="files">
        <label class="version">
            <span>Version</span>
            <select :value="version" @change="chose">
                <option v-for="v in mod.versions" :key="v.version" :value="v.version">
                    v{{ v.version }}{{ v.version === mod.latestVersion ? " · latest" : "" }}
                </option>
            </select>
        </label>
        <ul class="tree">
            <registry-detail-tree-node
                v-for="node in tree"
                :key="node.path"
                :node="node"
                :depth="0"
                :open="open"
                :active="file"
                @toggle="emit('toggle', $event)"
                @pick="emit('pick', $event)"
            />
        </ul>
    </section>
</template>

<style scoped lang="scss">
.files {
    padding: 1rem 0.5rem 0.75rem;
}

.version {
    display: flex;
    gap: 0.6rem;
    align-items: center;
    margin: 0 0.5rem 0.75rem;
    font-family: $font-display;
    font-size: 0.6rem;
    text-transform: uppercase;
    color: $ink-faint;

    select {
        flex: 1;
        height: 2rem;
        padding: 0 0.5rem;
        font-family: $font-mono;
        font-size: 0.72rem;
        text-transform: none;
        color: $ink;
        background: transparent;
        border: 1px solid $glass-edge;
        border-radius: $radius;
        cursor: pointer;

        option {
            background: $night-0;
        }
    }
}

.tree {
    margin: 0;
    padding: 0;
    list-style: none;
}
</style>
