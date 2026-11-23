<script setup lang="ts">
/**
 * Where a Card will be: its frame and proportions, empty, so a shelf keeps its shape while it
 * loads or before it has anything on it. Still (no shimmer: the page re-uploads on every repaint).
 */
defineProps<{ wide?: boolean }>()
</script>

<template>
    <div class="slot" :class="{ wide }" aria-hidden="true">
        <div class="art" />
        <div v-if="!wide" class="body"><span class="mark" /><span class="lines"><i /><i class="short" /></span></div>
    </div>
</template>

<style scoped>
.slot { display: flex; flex-direction: column; min-width: 0; border: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.02); }
.art { flex: none; height: 0; padding-bottom: 56.25%; /* 16:9, as Card art */ border-bottom: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.025); }
.body { display: flex; gap: 0.75rem; align-items: center; padding: 0.9rem; min-height: 4.6rem; }
.mark { flex: none; width: 45px; height: 45px; margin-top: -2.2rem; border: 1px solid var(--glass-edge); background: rgb(4 7 11 / 0.85); }
.lines { display: grid; gap: 0.45rem; flex: 1; }
i { display: block; height: 0.55rem; width: 70%; background: rgb(231 237 244 / 0.06); }
i.short { width: 40%; background: rgb(231 237 244 / 0.04); }
/* the size of the featured banner */
.slot.wide { min-height: 300px; }
.slot.wide > .art { flex: 1; padding-bottom: 0; border-bottom: 0; }
</style>
