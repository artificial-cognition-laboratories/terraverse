<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { RegistryMod } from "../../bridge/index.ts"
import { Button, Markdown, Notice, PageHead, Spinner } from "../../kit/index.ts"
import { format } from "./format.ts"
import ModAside from "./ModAside.vue"
import ModSource from "./ModSource.vue"
import ModVersions from "./ModVersions.vue"
import { useRegistry } from "./registry.ts"
import { Request } from "./request.ts"

/** One mod or texture pack's own page: a head on the page, then its README, source, versions and dependencies. */
type Tab = "overview" | "source" | "versions" | "dependencies"
const props = defineProps<{ name: string; back: string }>()
const emit = defineEmits<{ back: []; open: [name: string] }>()
const registry = useRegistry()
const mod = Request<string, RegistryMod>((name) => registry.mod(name))
const tab = ref<Tab>("overview")
const file = ref<string | null>(null)
const latest = computed(() => mod.data.value?.versions[0] ?? null)
const dependencies = computed(() => Object.entries(latest.value?.dependencies ?? {}))
const tabs = computed(() => [
    { id: "overview" as const, label: "Overview" },
    ...(mod.data.value?.files ? [{ id: "source" as const, label: "Source", count: mod.data.value.files.length }] : []),
    { id: "versions" as const, label: "Versions", count: mod.data.value?.versions.length ?? null },
    { id: "dependencies" as const, label: "Dependencies", count: dependencies.value.length },
])
/** A README's relative media is in the version's assets: `./assets/kiln.png` is served as `<media>kiln.png`. */
function media(src: string): string {
    const base = mod.data.value?.media
    if (!base || /^[a-z]+:|^\//i.test(src)) return src
    return base + src.replace(/^(\.\/)?(assets\/)?/, "")
}
function openFile(path: string) {
    file.value = path
    tab.value = "source"
}
watch(() => props.name, (name) => { tab.value = "overview"; file.value = null; mod.run(name) }, { immediate: true })
</script>

<template>
    <div class="mod-page">
        <Notice v-if="mod.problem.value" tone="danger">{{ mod.problem.value }}</Notice>
        <div v-else-if="!mod.data.value" class="loading"><Spinner /> Reading {{ name }}…</div>
        <template v-else>
            <div class="glow" />
            <PageHead v-model:tab="tab" :title="mod.data.value.name" :mark="mod.data.value.name" :image="mod.data.value.icon"
                :badge="mod.data.value.latestVersion" :description="mod.data.value.description" :back="back" :tabs="tabs"
                @back="emit('back')">
                <template #meta>
                    <span>by <b>{{ mod.data.value.owner }}</b></span>
                    <span><b>★ {{ format.count(mod.data.value.stars) }}</b> stars</span>
                    <span v-if="mod.data.value.installs !== null"><b>↓ {{ format.count(mod.data.value.installs) }}</b> installs</span>
                    <span>updated <b>{{ format.ago(mod.data.value.updatedAt) }}</b></span>
                </template>
                <template #actions>
                    <Button v-if="mod.data.value.installed" disabled title="Not wired yet">Installed</Button>
                    <Button v-else variant="primary" disabled title="Not wired yet">Install »</Button>
                </template>
            </PageHead>
            <Notice v-if="mod.data.value.deprecated !== null" tone="warn">
                <strong>Deprecated.</strong> {{ mod.data.value.deprecated || "Its author no longer maintains it." }}
            </Notice>
            <div class="body">
                <div class="main">
                    <Markdown v-if="tab === 'overview'" :source="mod.data.value.readme || '*No README.*'" :resolve="media" />
                    <ModSource v-else-if="tab === 'source'" v-model="file" :mod="mod.data.value" />
                    <ModVersions v-else-if="tab === 'versions'" :versions="mod.data.value.versions" />
                    <div v-else class="deps">
                        <p v-if="!dependencies.length" class="dim">Needs nothing else.</p>
                        <button v-for="[dep, version] in dependencies" :key="dep" type="button" class="dep" @click="emit('open', dep)">
                            <span>{{ dep }}</span><code>{{ version }}</code>
                        </button>
                    </div>
                </div>
                <ModAside v-if="tab === 'overview'" :mod="mod.data.value" @file="openFile" />
            </div>
        </template>
    </div>
</template>

<style scoped>
/* the same width as the browse pages: 220 + 32 + 600, plus the frame's padding */
.mod-page { position: relative; display: grid; align-content: start; gap: 20px; max-width: 904px; margin: 0 auto; padding: 22px 26px 40px; }
/* fades out well above the tabs' rule, so no edge shows where it ends */
.glow { position: absolute; inset: 0 0 auto; height: 200px; background: radial-gradient(ellipse 50% 100% at 50% 0%, rgb(0 148 210 / 0.12), transparent 100%); pointer-events: none; }
.loading { display: flex; gap: 10px; align-items: center; color: var(--ink-dim); font-size: 12px; }
.body { display: grid; grid-template-columns: minmax(0, 1fr) 200px; gap: 32px; align-items: start; }
.main { min-width: 0; }
.main :deep(.markdown) { max-width: none; }
.body:has(.main:only-child) { grid-template-columns: minmax(0, 1fr); }
.deps { display: grid; gap: 6px; max-width: 560px; }
.dep { display: flex; justify-content: space-between; gap: 12px; padding: 9px 12px; border: 1px solid var(--stone-3); background: none; color: var(--ink); font-size: 12px; text-align: left; }
.dep:hover { border-color: var(--accent-lo); }
.dep code { color: var(--accent-hi); }
.dim { color: var(--ink-faint); font-size: 12px; }
</style>
