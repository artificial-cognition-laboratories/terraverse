<script setup lang="ts">
import type { RegistryVersion } from "../../bridge/index.ts"
import { format } from "./format.ts"

/** Every published version, newest first, with the API it targets and any reason it's withheld. */
defineProps<{ versions: readonly RegistryVersion[] }>()
</script>

<template>
    <table class="versions">
        <thead><tr><th>Version</th><th>Mod API</th><th>Dependencies</th><th>Published</th></tr></thead>
        <tbody>
            <tr v-for="(v, i) in versions" :key="v.version" :class="{ withheld: v.withheld }">
                <td><code>{{ v.version }}</code><span v-if="i === 0" class="tag">latest</span><span v-if="v.withheld" class="tag warn" :title="v.withheld">withheld</span></td>
                <td>v{{ v.api }}</td>
                <td>{{ Object.keys(v.dependencies).length }}</td>
                <td>{{ format.ago(v.publishedAt) }}</td>
            </tr>
        </tbody>
    </table>
</template>

<style scoped>
.versions { width: 100%; max-width: 720px; border-collapse: collapse; font-size: 11px; }
th { padding: 6px 10px; border-bottom: 1px solid var(--stone-3); color: var(--ink-faint); font: 9px var(--font-display); text-align: left; text-transform: uppercase; }
td { padding: 8px 10px; border-bottom: 1px solid var(--stone-2); color: var(--ink-dim); }
code { color: var(--ink); }
.tag { margin-left: 8px; padding: 1px 5px; border: 1px solid var(--accent-lo); color: var(--accent-hi); font-size: 9px; }
.tag.warn { border-color: var(--warn-lo); color: #e0b33d; }
.withheld code { text-decoration: line-through; color: var(--ink-faint); }
</style>
