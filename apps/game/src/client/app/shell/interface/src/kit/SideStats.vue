<script setup lang="ts">
import PixelIcon from "../game/PixelIcon.vue"
import type { Icon } from "./icons.ts"

/** A row of headline numbers in a side card, split by hairlines. */
defineProps<{ items: readonly { icon: Icon; value: string; label: string }[] }>()
</script>

<template>
    <div class="side-stats flush">
        <div v-for="item in items" :key="item.label">
            <PixelIcon :rows="item.icon.rows" :palette="item.icon.palette" style="--u: 1.4px" />
            <strong>{{ item.value }}</strong>
            <span>{{ item.label }}</span>
        </div>
    </div>
</template>

<style scoped>
.side-stats { display: grid; grid-auto-columns: 1fr; grid-auto-flow: column; padding: 0.9rem 0; }
div > div { display: grid; justify-items: center; gap: 0.25rem; }
div > div + div { border-left: 1px solid var(--glass-edge); }
strong { color: var(--ink); font-size: 0.95rem; }
span { color: var(--ink-faint); font-size: 0.62rem; }
</style>
