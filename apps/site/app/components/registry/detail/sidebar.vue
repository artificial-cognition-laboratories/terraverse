<script setup lang="ts">
import type { Mod } from "~/registry"
import { useAccount } from "~/account"
import type { Node } from "~/registry/tree"
import { contents, numbers, type Tab } from "~/registry/detail"

/**
 * A mod page's side card. Navigation first (what to look at), then reference
 * (the numbers, the particulars), then a version's files. A phone has no side card: the page
 * places these pieces itself (detail/index.vue).
 */
const props = defineProps<{ mod: Mod; tab: Tab; version: string; tree: readonly Node[]; open: ReadonlySet<string>; file: string | null }>()
const emit = defineEmits<{ tab: [tab: Tab]; version: [version: string]; toggle: [path: string]; pick: [path: string] }>()

const account = useAccount()
const tabs = computed(() => contents(props.mod, props.version))
const stats = computed(() => numbers(props.mod, account.stars.has(props.mod.name)))
</script>

<template>
    <shared-side class="sidebar">
        <shared-side-nav :items="tabs" :active="tab" @pick="emit('tab', $event)" />
        <shared-side-stats :items="stats" />
        <registry-detail-about :mod="mod" />
        <registry-detail-files
            class="flush"
            :mod="mod"
            :version="version"
            :tree="tree"
            :open="open"
            :file="tab === 'source' ? file : null"
            @version="emit('version', $event)"
            @toggle="emit('toggle', $event)"
            @pick="emit('pick', $event)"
        />
    </shared-side>
</template>
