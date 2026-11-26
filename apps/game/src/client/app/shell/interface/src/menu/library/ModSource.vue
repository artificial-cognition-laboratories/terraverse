<script setup lang="ts">
import { computed, watch } from "vue"
import type { RegistryMod } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { KeptNote, Notice, Spinner } from "../../kit/index.ts"
import { format } from "./format.ts"

/**
 * One file of the latest version, as the website's (registry/detail/source.vue): a window, its path
 * and size along the bar, its text under it. The file is picked in the side card's tree.
 */
const props = defineProps<{ mod: RegistryMod; file: string }>()
const text = useCache().cached("registry.file")
const IMAGE = /\.(png|jpe?g|gif|webp)$/i
const crumbs = computed(() => props.file.split("/"))
const size = computed(() => props.mod.files?.find((f) => f.path === props.file)?.size ?? null)
watch(() => props.file, (path) => { if (!IMAGE.test(path)) void text.run({ name: props.mod.name, path: path }) }, { immediate: true })
</script>

<template>
    <figure class="source">
        <figcaption>
            <span class="dots" aria-hidden="true"><i /><i /><i /></span>
            <span class="path">
                <template v-for="(part, i) in crumbs" :key="i"><span v-if="i" class="slash">/</span><span :class="{ last: i === crumbs.length - 1 }">{{ part }}</span></template>
            </span>
            <span v-if="size !== null" class="size">{{ format.size(size) }}</span>
        </figcaption>
        <p v-if="IMAGE.test(file)" class="binary">An image: it shows in the README, and in the game.</p>
        <Notice v-else-if="text.problem.value && text.data.value === null" tone="danger">{{ text.problem.value }}</Notice>
        <div v-else-if="text.data.value === null" class="binary"><Spinner /></div>
        <template v-else>
            <KeptNote v-if="text.problem.value && text.at.value !== null" :at="text.at.value" :problem="text.problem.value" />
            <pre><code>{{ text.data.value }}</code></pre>
        </template>
    </figure>
</template>

<style scoped>
.source { display: flex; flex-direction: column; min-width: 0; min-height: 24rem; margin: 0; border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); }
figcaption { display: flex; align-items: center; gap: 1rem; min-height: 2.8rem; padding: 0 1rem; white-space: nowrap; overflow: hidden; }
.dots { display: flex; flex: none; gap: 0.4rem; }
.dots i { width: 0.6rem; height: 0.6rem; border-radius: 50%; background: var(--night-4); }
.path { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; color: var(--ink-faint); font: 0.74rem var(--font); }
.slash { margin: 0 0.3rem; }
.last { color: var(--ink); }
.size { color: var(--ink-faint); font: 0.7rem var(--font); }
pre { margin: 0; padding: 0 1.25rem 1.1rem; overflow-x: auto; color: var(--ink); font: 0.8rem/1.65 var(--font); tab-size: 4; user-select: text; }
pre code { font: inherit; }
.binary { margin: 0; padding: 3rem 1.5rem; color: var(--ink-faint); font-size: 0.85rem; text-align: center; }
</style>
