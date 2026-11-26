<script setup lang="ts">
/** Where a list row will be: its height and lines, empty, while the list is first read. Still. */
defineProps<{ count?: number }>()
</script>

<template>
    <div class="rows" aria-hidden="true">
        <div v-for="i in count ?? 4" :key="i" class="slot">
            <span class="thumb" /><span class="lines"><i /><i class="short" /></span><i class="end" />
        </div>
    </div>
</template>

<style scoped>
.rows { display: grid; gap: 0.75rem; }
.slot { display: grid; grid-template-columns: 7rem minmax(0, 1fr) 6rem; gap: 1.2rem; align-items: center; height: 4rem; padding-right: 1.2rem; border: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.02); }
.thumb { height: 100%; border-right: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.025); }
.lines { display: grid; gap: 0.45rem; }
i { display: block; height: 0.55rem; width: 55%; background: rgb(231 237 244 / 0.06); }
i.short { width: 35%; background: rgb(231 237 244 / 0.04); }
i.end { width: 100%; background: rgb(231 237 244 / 0.04); }
</style>
