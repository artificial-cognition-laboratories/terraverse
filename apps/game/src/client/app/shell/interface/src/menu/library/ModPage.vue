<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { LocalMod, RegistryMod } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { Button, KeptNote, Markdown, Notice, PageSlot } from "../../kit/index.ts"
import { ICONS } from "../../kit/icons.ts"
import ModContents from "./ModContents.vue"
import ModDependencies from "./ModDependencies.vue"
import ModHeader from "./ModHeader.vue"
import ModSide from "./ModSide.vue"
import ModSource from "./ModSource.vue"
import ModVersions from "./ModVersions.vue"

/**
 * One mod or texture pack's own page, as the website's (registry/detail/index.vue): the header and
 * what's chosen in the main column, the side card beside it. The same page for a registry mod (by
 * name) and one you're making (`local`, from your profile's mods): only what each has is shown.
 */
type Tab = "readme" | "contents" | "versions" | "dependencies" | "source"
const props = withDefaults(defineProps<{ name: string; back: string; local?: LocalMod | null }>(), { local: null })
const emit = defineEmits<{ back: []; open: [name: string] }>()
const fetched = useCache().cached("registry.mod")
const mod = computed(() => props.local ?? fetched.data.value)
const tab = ref<Tab>("readme")
const file = ref<string | null>(null)
const reload = ref(0)
const count = ref<number | null>(null)
const needs = computed(() => Object.entries(props.local?.dependencies ?? mod.value?.versions[0]?.dependencies ?? {}))
const tabs = computed(() => [
    // (a published mod's blocks will need its package fetched first: not built yet)
    ...(props.local ? [{ id: "contents" as const, label: "Blocks & items", icon: ICONS.blocks, count: count.value }] : []),
    { id: "readme" as const, label: "Readme", icon: ICONS.readme },
    ...(props.local ? [] : [{ id: "versions" as const, label: "Versions", icon: ICONS.versions, count: mod.value?.versions.length ?? null }]),
    { id: "dependencies" as const, label: "Dependencies", icon: ICONS.dependencies, count: needs.value.length },
    ...(mod.value?.files?.length ? [{ id: "source" as const, label: "Source", icon: ICONS.source, count: mod.value.files.length }] : []),
])
/** A README's relative media is in the version's assets: `./assets/kiln.png` is served as `<media>kiln.png`. */
function media(src: string): string {
    const base = mod.value?.media
    if (!base || /^[a-z]+:|^\//i.test(src)) return src
    return base + src.replace(/^(\.\/)?(assets\/)?/, "")
}
function openFile(path: string) {
    file.value = path
    tab.value = "source"
}
// the source opens on the server's entry, as the website's does
watch(tab, (t) => {
    const files = mod.value?.files ?? []
    if (t === "source" && !file.value) file.value = (files.find((f) => f.path === "server/main.ts") ?? files[0])?.path ?? null
})
watch(() => [props.name, props.local?.path] as const, ([name]) => {
    tab.value = props.local ? "contents" : "readme"
    file.value = null
    count.value = null
    if (!props.local) void fetched.run({ name: name })
}, { immediate: true })
</script>

<template>
    <div class="mod-page">
        <button type="button" class="back" @click="emit('back')">← {{ back }}</button>
        <Notice v-if="!mod && fetched.problem.value" tone="danger">{{ fetched.problem.value }}</Notice>
        <PageSlot v-else-if="!mod" />
        <KeptNote v-if="mod && !local && fetched.problem.value && fetched.at.value !== null" :at="fetched.at.value" :problem="fetched.problem.value" />
        <div v-if="mod" class="detail">
            <main class="main">
                <ModHeader :mod="mod" :yours="local !== null">
                    <template #actions>
                        <Button v-if="local" size="sm" title="Read the mod again after you save" @click="reload++">Reload</Button>
                        <Button v-else-if="mod.installed" size="sm" disabled title="Not wired yet">Installed</Button>
                        <Button v-else variant="primary" size="sm" disabled title="Not wired yet">Install »</Button>
                    </template>
                </ModHeader>
                <ModContents v-if="tab === 'contents' && local" :path="local.path" :reload="reload" @count="(n) => (count = n)" />
                <Markdown v-else-if="tab === 'readme'" :source="readme(mod)" :resolve="media" />
                <ModVersions v-else-if="tab === 'versions'" :versions="mod.versions" />
                <ModDependencies v-else-if="tab === 'dependencies'" :needs="needs" @open="(n) => emit('open', n)" />
                <ModSource v-else-if="tab === 'source' && file" :mod="mod" :file="file" />
            </main>
            <ModSide class="side" v-model:tab="tab" :mod="mod" :tabs="tabs" :file="tab === 'source' ? file : null" @file="openFile" />
        </div>
    </div>
</template>

<script lang="ts">
/** The README to show: the header above already says the mod's name, so its opening title goes. */
function readme(mod: RegistryMod & { path?: string }): string {
    if (!mod.readme) return mod.path ? `*No README yet: add a README.md to ${mod.path}.*` : "*No README.*"
    return mod.readme.replace(/^\s*# [^\n]*\n+/, "")
}
</script>

<style scoped>
.mod-page { display: grid; gap: 1rem; }
.back { justify-self: start; padding: 0; border: 0; background: none; color: var(--ink-faint); font: 0.62rem var(--font-display); text-transform: uppercase; }
.back:hover { color: var(--ink); }
/* the side card on the right, as npm and GitHub: the readme leads, reference sits beside it */
.detail { display: grid; grid-template-columns: minmax(0, 1fr) 17rem; gap: 2.5rem; align-items: start; }
.main { display: grid; gap: 2rem; min-width: 0; }
/* stays put while the readme scrolls, as Browse's side card does */
.side { position: sticky; top: 2.5rem; }
@media (max-width: 900px) { .detail { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
