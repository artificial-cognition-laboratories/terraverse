<script setup lang="ts">
import { CATEGORY_LABELS, type Mod } from "~/registry"
import { useAccount } from "~/account"
import type { Node } from "~/registry/tree"
import type { Tab } from "~/registry/detail"
import { format } from "~/lib/format"

/**
 * A mod page's side card. Navigation first (what to look at), then reference
 * (the numbers, the dates, where it's filed), then a version's files.
 */
const props = defineProps<{ mod: Mod; tab: Tab; version: string; tree: readonly Node[]; open: ReadonlySet<string>; file: string | null }>()
const emit = defineEmits<{ tab: [tab: Tab]; version: [version: string]; toggle: [path: string]; pick: [path: string] }>()

const account = useAccount()
const needs = computed(() => Object.keys(props.mod.versions.find(v => v.version === props.version)?.dependencies ?? {}).length)
const contents = computed(() => [
    { id: "readme" as const, label: "Readme", icon: "readme" as const },
    { id: "versions" as const, label: "Versions", icon: "versions" as const, count: props.mod.versions.length },
    { id: "dependencies" as const, label: "Dependencies", icon: "dependencies" as const, count: needs.value + props.mod.dependents.length },
    { id: "source" as const, label: "Source", icon: "source" as const, count: props.mod.files.length },
])
const stats = computed(() => [
    { icon: "download" as const, value: format.count(props.mod.installs), label: "installs" },
    { icon: "star" as const, value: format.count(props.mod.stars + (account.stars.has(props.mod.name) ? 1 : 0)), label: "stars" },
    { icon: "dependencies" as const, value: format.count(props.mod.dependents.length), label: "dependents" },
])

function chose(event: Event) {
    if (event.target instanceof HTMLSelectElement) emit("version", event.target.value)
}
</script>

<template>
    <shared-side class="sidebar">
        <shared-side-nav :items="contents" :active="tab" @pick="emit('tab', $event)" />
        <shared-side-stats :items="stats" />

        <dl class="meta">
            <div><dt>Latest</dt><dd>v{{ mod.latestVersion }}</dd></div>
            <div><dt>Updated</dt><dd>{{ format.date(mod.updatedAt) }}</dd></div>
            <div><dt>Published</dt><dd>{{ format.date(mod.createdAt) }}</dd></div>
            <div><dt>Game API</dt><dd>{{ mod.versions[0]?.api }}</dd></div>
            <div><dt>License</dt><dd>{{ mod.license }}</dd></div>
            <div>
                <dt>{{ mod.categories.length === 1 ? "Category" : "Categories" }}</dt>
                <dd class="filed">
                    <NuxtLink v-for="c in mod.categories" :key="c" :to="{ path: '/registry/browse', query: { category: c } }">
                        {{ CATEGORY_LABELS[c] }}
                    </NuxtLink>
                </dd>
            </div>
        </dl>

        <section class="files flush">
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
                    :active="tab === 'source' ? file : null"
                    @toggle="emit('toggle', $event)"
                    @pick="emit('pick', $event)"
                />
            </ul>
        </section>
    </shared-side>
</template>

<style scoped lang="scss">
.meta {
    display: grid;
    gap: 0.45rem;
    font-size: 0.75rem;

    div {
        display: flex;
        justify-content: space-between;
        gap: 1rem;
    }

    dt {
        color: $ink-faint;
    }

    dd {
        margin: 0;
        color: $ink-dim;
        text-align: right;
    }
}

.filed {
    display: flex;
    flex-wrap: wrap;
    justify-content: flex-end;
    gap: 0.2rem 0.6rem;

    a {
        color: $primary-ink;
        text-decoration: none;

        &:hover {
            color: $ink;
        }
    }
}

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
