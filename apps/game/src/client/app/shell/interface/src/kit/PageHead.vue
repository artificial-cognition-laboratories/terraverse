<script setup lang="ts" generic="T extends string">
import Monogram from "./Monogram.vue"

/**
 * The head of a thing's own page: no card, it sits on the page. A large mark at the left,
 * the name, its version and a line about it; actions at the right; the page's tabs run
 * along its bottom border.
 */
withDefaults(defineProps<{
    title: string; mark: string; image?: string | null; badge?: string | null
    description?: string | null; back?: string | null
    tabs: readonly { id: T; label: string; count?: number | null }[]
}>(), { image: null, badge: null, description: null, back: null })
const tab = defineModel<T>("tab", { required: true })
defineEmits<{ back: [] }>()
</script>

<template>
    <header class="page-head">
        <button v-if="back" type="button" class="back" @click="$emit('back')">← {{ back }}</button>
        <div class="top">
            <Monogram :name="mark" :image="image" :size="76" />
            <div class="names">
                <div class="title"><h1>{{ title }}</h1><span v-if="badge" class="badge">{{ badge }}</span></div>
                <p v-if="description" class="description">{{ description }}</p>
                <div v-if="$slots.meta" class="meta"><slot name="meta" /></div>
            </div>
            <div v-if="$slots.actions" class="actions"><slot name="actions" /></div>
        </div>
        <nav class="tabs" role="tablist">
            <button v-for="t in tabs" :key="t.id" type="button" role="tab" :aria-selected="tab === t.id" :class="{ on: tab === t.id }" @click="tab = t.id">
                {{ t.label }}<span v-if="t.count != null" class="count">{{ t.count }}</span>
            </button>
        </nav>
    </header>
</template>

<style scoped>
.page-head { display: grid; gap: 14px; min-width: 0; border-bottom: var(--b) solid var(--stone-3); }
.back { justify-self: start; padding: 0; border: 0; background: none; color: var(--ink-faint); font-size: 11px; }
.back:hover { color: var(--accent-hi); }
.top { display: grid; grid-template-columns: auto minmax(0, 1fr) auto; gap: 18px; align-items: center; min-width: 0; }
.names { display: grid; gap: 7px; min-width: 0; }
.title { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; min-width: 0; }
h1 { margin: 0; overflow: hidden; font: 22px var(--font-display); text-overflow: ellipsis; white-space: nowrap; }
.badge { padding: 2px 6px; border: 1px solid var(--accent-lo); border-radius: var(--radius); color: var(--accent-hi); font-size: 10px; }
.description { margin: 0; max-width: 80ch; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
.meta { display: flex; flex-wrap: wrap; gap: 16px; color: var(--ink-faint); font-size: 10px; }
.meta :deep(b) { color: var(--ink); font-weight: normal; }
.actions { display: flex; gap: 8px; align-items: center; }
.tabs { display: flex; gap: 2px; margin-bottom: calc(var(--b) * -1); }
.tabs button { display: flex; gap: 6px; align-items: center; height: 34px; padding: 0 14px; border: 0; border-bottom: var(--b) solid transparent; background: none; color: var(--ink-faint); font: 10px var(--font-display); text-transform: uppercase; white-space: nowrap; }
.tabs button:hover { color: var(--ink); }
.tabs button.on { border-bottom-color: var(--accent); color: var(--ink); }
.count { padding: 1px 4px; background: var(--stone-3); color: var(--ink-dim); font: 9px var(--font); }
</style>
