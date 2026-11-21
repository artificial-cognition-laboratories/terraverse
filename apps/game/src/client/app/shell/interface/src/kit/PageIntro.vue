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
.page-intro { display: grid; gap: 7px; padding-bottom: 16px; border-bottom: 1px solid var(--stone-4); }
h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
p { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
</style>
