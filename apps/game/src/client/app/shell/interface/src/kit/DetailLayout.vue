<script setup lang="ts">
import Notice from "./Notice.vue"
import PageSlot from "./PageSlot.vue"

/**
 * A thing's own page laid out as the website's registry detail: the way back (none when `back`
 * is null: a page of its own, as yours is), then the main
 * column (its header and the chosen section) with the side card beside it, held while the main
 * column scrolls. `ready` false draws the page's slot, or `problem` when it couldn't be read.
 */
defineProps<{ back: string | null; ready: boolean; problem?: string | null }>()
defineEmits<{ back: [] }>()
</script>

<template>
    <div class="detail-page">
        <button v-if="back" type="button" class="back" @click="$emit('back')">← {{ back }}</button>
        <Notice v-if="!ready && problem" tone="danger">{{ problem }}</Notice>
        <PageSlot v-else-if="!ready" />
        <slot name="above" />
        <div v-if="ready" class="detail">
            <main class="main"><slot /></main>
            <div class="side"><slot name="side" /></div>
        </div>
    </div>
</template>

<style scoped>
.detail-page { display: grid; gap: 1rem; }
.back { justify-self: start; padding: 0; border: 0; background: none; color: var(--ink-faint); font: 0.62rem var(--font-display); text-transform: uppercase; }
.back:hover { color: var(--ink); }
/* the side card on the right, as npm and GitHub: the page leads, reference sits beside it */
.detail { display: grid; grid-template-columns: minmax(0, 1fr) 17rem; gap: 2.5rem; align-items: start; }
.main { display: grid; gap: 2rem; min-width: 0; }
.side { position: sticky; top: 2.5rem; }
@media (max-width: 900px) { .detail { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
