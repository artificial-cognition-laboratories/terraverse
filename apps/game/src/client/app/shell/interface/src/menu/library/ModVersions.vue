<script setup lang="ts">
import type { RegistryVersion } from "../../bridge/index.ts"
import { format } from "./format.ts"

/**
 * Every published version, newest first, as the website's table (registry/detail/versions.vue):
 * when, against which game API, what it needs, and whether it was withheld.
 */
defineProps<{ versions: readonly RegistryVersion[] }>()
</script>

<template>
    <div class="versions">
        <table>
            <thead><tr><th>Version</th><th>Published</th><th>Game API</th><th>Needs</th></tr></thead>
            <tbody>
                <tr v-for="(v, i) in versions" :key="v.version" :class="{ on: i === 0 }">
                    <td class="version">
                        v{{ v.version }}
                        <span v-if="i === 0" class="latest">latest</span>
                        <span v-if="v.withheld" class="withheld" :title="v.withheld">withheld</span>
                    </td>
                    <td>{{ format.date(v.publishedAt) }}</td>
                    <td>{{ v.api }}</td>
                    <td>{{ Object.keys(v.dependencies).length || "nothing" }}</td>
                </tr>
            </tbody>
        </table>
    </div>
</template>

<style scoped>
.versions { overflow-x: auto; border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); }
table { width: 100%; border-collapse: collapse; font-size: 0.8rem; }
th { padding: 0.85rem 1.1rem; border-bottom: 1px solid var(--glass-edge); color: var(--ink-faint); font: 400 0.6rem var(--font-display); text-align: left; text-transform: uppercase; }
td { padding: 0.85rem 1.1rem; border-bottom: 1px solid rgb(231 237 244 / 0.05); color: var(--ink-dim); white-space: nowrap; }
tr:last-child td { border-bottom: 0; }
tr.on td { background: rgb(10 75 145 / 0.15); }
.version { color: var(--ink); font-family: var(--font); }
.latest, .withheld { margin-left: 0.5rem; padding: 0.1rem 0.35rem; border: 1px solid rgb(94 186 129 / 0.5); color: var(--alive); font-size: 0.62rem; }
.withheld { border-color: rgb(210 153 34 / 0.5); color: var(--warn); }
</style>
