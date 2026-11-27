<script setup lang="ts">
import type { PublicServer } from "../bridge/index.ts"

/**
 * Servers as the website's table: name, what it plays as, mods, region and how full. `empty`: the
 * one row it shows instead when there are none (why, or that it's still asking).
 */
defineProps<{ servers: readonly PublicServer[]; empty?: string | null }>()
defineEmits<{ open: [server: PublicServer] }>()
</script>

<template>
    <table class="live">
        <thead><tr><th>Server</th><th>Plays as</th><th>Mods</th><th>Region</th><th class="right">Players</th></tr></thead>
        <tbody>
            <tr v-if="!servers.length" class="none"><td colspan="5">{{ empty ?? "No public servers yet." }}</td></tr>
            <tr v-for="s in servers" :key="s.id" @click="$emit('open', s)">
                <td class="name"><span class="dot" :class="{ idle: !s.players }" />{{ s.name }}</td>
                <td>{{ s.tags[0] ?? "—" }}</td>
                <td>{{ s.mods.length }}</td>
                <td>{{ s.region }}</td>
                <td class="right"><span class="fill"><span :style="{ width: `${(s.players / s.capacity) * 100}%` }" /></span>{{ s.players }}/{{ s.capacity }}</td>
            </tr>
        </tbody>
    </table>
</template>

<style scoped>
.live { width: 100%; border: 1px solid var(--glass-edge); border-radius: var(--radius); border-collapse: collapse; background: var(--glass); backdrop-filter: blur(8px); font-size: 0.78rem; }
th { padding: 0.8rem 1rem; border-bottom: 1px solid var(--glass-edge); color: var(--ink-faint); font: 400 0.6rem var(--font-display); text-align: left; text-transform: uppercase; }
td { padding: 0.85rem 1rem; border-bottom: 1px solid rgb(231 237 244 / 0.05); color: var(--ink-dim); white-space: nowrap; }
tr:last-child td { border-bottom: none; }
tbody tr { cursor: pointer; }
tbody tr:hover td { background: rgb(231 237 244 / 0.03); color: var(--ink); }
.right { text-align: right; }
.name { color: var(--ink); font-weight: 700; }
/* green: it's alive */
.dot { display: inline-block; width: 0.45rem; height: 0.45rem; margin-right: 0.7rem; background: var(--alive); }
.dot.idle { background: var(--night-4); }
.none td { color: var(--ink-faint); cursor: default; }
tbody tr.none:hover td { background: none; color: var(--ink-faint); }
.fill { display: inline-block; width: 3.5rem; height: 0.4rem; margin-right: 0.6rem; background: var(--night-3); vertical-align: middle; }
.fill span { display: block; height: 100%; background: var(--accent-hi); }
</style>
