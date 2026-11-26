<script setup lang="ts">
/**
 * Where a detail page will be: its banner, a column of text and the side card beside it, empty,
 * while it's first read. The same grid as the page, so nothing moves when it fills. Still.
 */
</script>

<template>
    <div class="page" aria-hidden="true">
        <main>
            <div class="head"><span class="mark" /><span class="lines"><i class="title" /><i /></span></div>
            <div class="text"><i v-for="n in 7" :key="n" :class="{ short: n % 3 === 0 }" /></div>
        </main>
        <aside><i v-for="n in 5" :key="n" /></aside>
    </div>
</template>

<style scoped>
.page { display: grid; grid-template-columns: minmax(0, 1fr) 17rem; gap: 2.5rem; align-items: start; }
main { display: grid; gap: 2rem; min-width: 0; }
.head { display: flex; gap: 1.2rem; align-items: center; min-height: 9rem; padding: 1.5rem; border: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.02); }
.mark { flex: none; width: 64px; height: 64px; border: 1px solid var(--glass-edge); background: rgb(4 7 11 / 0.85); }
.lines, .text, aside { display: grid; gap: 0.7rem; flex: 1; }
aside { padding: 1.2rem; border: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.02); }
i { display: block; height: 0.6rem; width: 80%; background: rgb(231 237 244 / 0.05); }
i.title { height: 1rem; width: 45%; background: rgb(231 237 244 / 0.08); }
i.short { width: 50%; }
@media (max-width: 900px) { .page { grid-template-columns: minmax(0, 1fr); } }
</style>
