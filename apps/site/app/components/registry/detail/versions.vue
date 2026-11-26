<script setup lang="ts">
import type { Mod } from "~/registry"
import { format } from "~/lib/format"

/** Every version, newest first: when, against which game API, what it needs, and whether it was pulled. */
defineProps<{ mod: Mod; version: string }>()
const emit = defineEmits<{ pick: [version: string] }>()
</script>

<template>
    <div class="versions">
        <table>
            <thead>
                <tr><th>Version</th><th>Published</th><th>Game API</th><th>Needs</th><th /></tr>
            </thead>
            <tbody>
                <tr v-for="v in mod.versions" :key="v.version" :class="{ on: v.version === version }">
                    <td class="version">
                        v{{ v.version }}
                        <span v-if="v.version === mod.latestVersion" class="latest">latest</span>
                        <span v-if="v.withheld" class="withheld" :title="v.withheld">withheld</span>
                    </td>
                    <td>{{ format.date(v.publishedAt) }}</td>
                    <td>{{ v.api }}</td>
                    <td>{{ Object.keys(v.dependencies).length || "nothing" }}</td>
                    <td class="right">
                        <button type="button" @click="emit('pick', v.version)">{{ v.version === version ? "viewing" : "view" }}</button>
                    </td>
                </tr>
            </tbody>
        </table>
    </div>
</template>

<style scoped lang="scss">
.versions {
    @include panel;
    overflow-x: auto;
}

table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.8rem;
}

th {
    padding: 0.85rem 1.1rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.6rem;
    text-transform: uppercase;
    text-align: left;
    color: $ink-faint;
    border-bottom: 1px solid $glass-edge;
}

td {
    padding: 0.85rem 1.1rem;
    color: $ink-dim;
    border-bottom: 1px solid rgba($ink, 0.05);
    white-space: nowrap;
}

tr:last-child td {
    border-bottom: none;
}

tr.on td {
    background: rgba($primary, 0.15);
}

.version {
    font-family: $font-mono;
    color: $ink;
}

.latest,
.withheld {
    margin-left: 0.5rem;
    padding: 0.1rem 0.35rem;
    font-size: 0.62rem;
    border: 1px solid rgba($forest-ink, 0.5);
    border-radius: $radius;
    color: $forest-ink;
}

.withheld {
    border-color: rgba($status-warn, 0.5);
    color: $status-warn;
}

.right {
    text-align: right;
}

button {
    font-family: $font-display;
    font-size: 0.58rem;
    text-transform: uppercase;
    color: $primary-ink;
    background: none;
    border: none;
    cursor: pointer;
}
</style>
