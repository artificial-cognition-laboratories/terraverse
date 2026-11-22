<script setup lang="ts">
/** A page's title and a line saying what it's for, ruled off from what follows. */
defineProps<{ title: string; note?: string | null }>()
</script>

<template>
    <header class="page-intro">
        <h2>{{ title }}</h2>
        <p v-if="note">{{ note }}</p>
        <slot />
    </header>
</template>

<style scoped>
/* the site's page head: a display title, a sans lede, a hairline under */
.page-intro { display: grid; gap: 0.4rem; padding-bottom: 1rem; border-bottom: 1px solid var(--glass-edge); }
h2 { margin: 0; font: 400 1.15rem var(--font-display); letter-spacing: 0.04em; text-transform: uppercase; color: var(--ink); }
p { margin: 0; max-width: 68ch; color: var(--ink-dim); font: 0.85rem/1.55 var(--font-sans); }
</style>
