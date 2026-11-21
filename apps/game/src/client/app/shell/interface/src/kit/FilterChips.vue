<script setup lang="ts">
/** The filters in force, each removable, so the current query is always on screen. */
defineProps<{ chips: readonly { id: string; label: string }[] }>()
defineEmits<{ remove: [id: string]; clear: [] }>()
</script>

<template>
    <div v-if="chips.length" class="chips">
        <button v-for="chip in chips" :key="chip.id" type="button" class="chip" :aria-label="`Remove ${chip.label}`" @click="$emit('remove', chip.id)">
            {{ chip.label }} <span>×</span>
        </button>
        <button v-if="chips.length > 1" type="button" class="clear" @click="$emit('clear')">Clear all</button>
    </div>
</template>

<style scoped>
.chips { display: flex; flex-wrap: wrap; gap: 6px; align-items: center; }
.chip { display: inline-flex; gap: 6px; align-items: center; height: 22px; padding: 0 7px; border: 1px solid var(--accent-lo); background: rgb(0 148 210 / 0.1); color: var(--accent-hi); font-size: 10px; white-space: nowrap; }
.chip span { color: var(--ink-dim); font-size: 13px; line-height: 1; }
.chip:hover { border-color: var(--accent); }
.chip:hover span { color: var(--ink); }
.clear { padding: 0 4px; border: 0; background: none; color: var(--ink-faint); font-size: 10px; text-decoration: underline; }
.clear:hover { color: var(--ink); }
</style>
