<script setup lang="ts">
import { view as views, type Tab, type View } from "~/registry/detail"
import { tree } from "~/registry/tree"
import type { Mod } from "~/registry"

/**
 * A mod's page: the orchestrator. It owns the view (tab, version, open file),
 * kept in the URL; the sidebar and each tab only render what it hands them.
 */
const props = defineProps<{ mod: Mod }>()
const route = useRoute()
const current = computed(() => views.read(route.query, props.mod))
const files = computed(() => tree(props.mod.files))
const file = computed(() => props.mod.files.find(f => f.path === current.value.file) ?? props.mod.files.find(f => f.path === "server/main.ts") ?? props.mod.files[0])

// Folders the visitor has opened; those leading to the open file start open
const open = ref(new Set<string>(["server"]))
watch(
    () => current.value.file,
    path => {
        const parts = (path ?? "").split("/").slice(0, -1)
        parts.forEach((_, i) => open.value.add(parts.slice(0, i + 1).join("/")))
    },
    { immediate: true },
)

function show(change: Partial<View>) {
    return navigateTo({ query: views.write({ ...current.value, ...change }, props.mod) }, { replace: true })
}
function toggle(path: string) {
    const next = new Set(open.value)
    if (!next.delete(path)) next.add(path)
    open.value = next
}
</script>

<template>
    <div class="detail">
        <main class="main">
            <registry-detail-header :mod="mod" :version="current.version" />
            <registry-detail-readme v-if="current.tab === 'readme'" :name="mod.name" :markdown="mod.readme" />
            <registry-detail-versions v-else-if="current.tab === 'versions'" :mod="mod" :version="current.version" @pick="(version: string) => show({ version: version })" />
            <registry-detail-dependencies v-else-if="current.tab === 'dependencies'" :mod="mod" :version="current.version" />
            <registry-detail-source v-else-if="file" :file="file" :version="current.version" />
        </main>
        <registry-detail-sidebar
            :mod="mod"
            :tab="current.tab"
            :version="current.version"
            :tree="files"
            :open="open"
            :file="file?.path ?? null"
            @tab="(tab: Tab) => show({ tab: tab })"
            @version="(version: string) => show({ version: version })"
            @toggle="toggle"
            @pick="(path: string) => show({ tab: 'source', file: path })"
        />
    </div>
</template>

<style scoped lang="scss">
.detail {
    display: grid;
    // the side card on the right, as npm and GitHub: the readme leads, reference sits beside it
    grid-template-columns: minmax(0, 1fr) $registry-side;
    gap: 2.5rem;
    align-items: start;
}

.main {
    display: grid;
    gap: 2rem;
    min-width: 0;
}

@include tablet {
    .detail {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
