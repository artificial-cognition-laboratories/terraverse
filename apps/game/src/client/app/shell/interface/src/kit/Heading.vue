<script setup lang="ts">
/** A page's head, as the website opens a section: the kicker, the claim, a line under it, and a link on the right. */
defineProps<{ kicker: string; title: string; lede?: string | null; more?: string | null }>()
defineEmits<{ more: [] }>()
</script>

<template>
    <header class="heading">
        <div class="words">
            <p class="kicker"><span aria-hidden="true">»</span> {{ kicker }}</p>
            <h2>{{ title }}</h2>
            <p v-if="lede" class="lede">{{ lede }}</p>
        </div>
        <button v-if="more" type="button" class="more" @click="$emit('more')">{{ more }} »</button>
    </header>
</template>

<style scoped>
.heading { display: flex; align-items: flex-end; justify-content: space-between; gap: 2rem; margin-bottom: 2.5rem; }
.words { max-width: 40rem; }
.kicker { margin: 0 0 0.9rem; font: 0.72rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; color: var(--accent-hi); }
h2 { margin: 0; font: 700 2.1rem/1.15 var(--font-sans); letter-spacing: -0.01em; color: var(--ink); }
.lede { margin: 1rem 0 0; font: 1rem/1.65 var(--font-sans); color: var(--ink-dim); }
.more { flex: none; padding: 0; border: 0; background: none; font: 0.7rem var(--font-display); text-transform: uppercase; color: var(--accent-hi); white-space: nowrap; }
.more:hover { color: var(--ink); }
</style>
