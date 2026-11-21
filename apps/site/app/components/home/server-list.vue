<script setup lang="ts">
import type { Server } from "~/config/home"

/** Servers people are playing on now: name, game, mods, region and how full. */
defineProps<{ servers: readonly Server[] }>()
</script>

<template>
    <table class="list">
        <thead>
            <tr><th>Server</th><th>Game</th><th>Mods</th><th>Region</th><th class="right">Players</th></tr>
        </thead>
        <tbody>
            <tr v-for="s in servers" :key="s.name">
                <td class="name"><span class="dot" />{{ s.name }}</td>
                <td>{{ s.game }}</td>
                <td>{{ s.mods }}</td>
                <td>{{ s.region }}</td>
                <td class="right">
                    <span class="fill"><span :style="{ width: `${(s.players / s.slots) * 100}%` }" /></span>
                    {{ s.players }}/{{ s.slots }}
                </td>
            </tr>
        </tbody>
    </table>
</template>

<style scoped lang="scss">
.list {
    @include panel;
    width: 100%;
    border-collapse: collapse;
    font-size: 0.78rem;

    th {
        padding: 0.8rem 1rem;
        font-family: $font-display;
        font-weight: 400;
        font-size: 0.6rem;
        text-transform: uppercase;
        text-align: left;
        color: $ink-faint;
        border-bottom: 1px solid $glass-edge;
    }

    td {
        padding: 0.85rem 1rem;
        color: $ink-dim;
        border-bottom: 1px solid rgba($ink, 0.05);
        white-space: nowrap;
    }

    tr:last-child td {
        border-bottom: none;
    }

    .right {
        text-align: right;
    }
}

.name {
    color: $ink;
    font-weight: 700;
}

// Green: it's alive
.dot {
    display: inline-block;
    width: 0.45rem;
    height: 0.45rem;
    margin-right: 0.7rem;
    background: $forest-ink;
}

.fill {
    display: inline-block;
    width: 3.5rem;
    height: 0.4rem;
    margin-right: 0.6rem;
    background: $night-3;
    vertical-align: middle;

    span {
        display: block;
        height: 100%;
        background: $primary-ink;
    }
}

@include mobile {
    .list {
        th:nth-child(3),
        td:nth-child(3),
        th:nth-child(4),
        td:nth-child(4) {
            display: none;
        }
    }

    .fill {
        display: none;
    }
}
</style>
