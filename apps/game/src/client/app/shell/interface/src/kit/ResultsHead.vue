<script setup lang="ts">
/** The head over a page of results, as the website's Browse: what's shown and how many, the controls on the right, a hairline under. */
defineProps<{ title: string; count?: string | null }>()
</script>

<template>
    <header class="results-head">
        <div>
            <h1>{{ title }}</h1>
            <p v-if="count">{{ count }}</p>
        </div>
        <div class="controls"><slot /></div>
    </header>
</template>

<style scoped>
.results-head { display: flex; align-items: flex-end; justify-content: space-between; gap: 1.5rem; margin-bottom: 1.5rem; padding-bottom: 1rem; border-bottom: 1px solid var(--glass-edge); }
h1 { margin: 0; color: var(--ink); font: 700 1.6rem var(--font-sans); }
p { margin: 0.3rem 0 0; color: var(--ink-faint); font-size: 0.75rem; }
.controls { display: flex; gap: 0.75rem; align-items: center; }
</style>
