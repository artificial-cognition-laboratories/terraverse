<script setup lang="ts">
/** The side panel for a selected row: a swatch and title, a close, and what it holds. */
defineProps<{ title: string; color?: string }>()
defineEmits<{ close: [] }>()
</script>

<template>
    <aside class="detail-pane">
        <header>
            <i v-if="color" :style="{ background: color }" />
            <b>{{ title }}</b>
            <button title="Close" aria-label="Close" @click="$emit('close')">×</button>
        </header>
        <div class="body"><slot /></div>
    </aside>
</template>

<style scoped>
.detail-pane { display: flex; flex-direction: column; flex: none; width: 260px; min-height: 0; border-left: 1px solid var(--dt-line); background: var(--dt-rail); font-size: 12px; }
header { display: flex; align-items: center; gap: 6px; flex: none; height: 28px; padding: 0 6px 0 10px; border-bottom: 1px solid var(--dt-line); }
header i { width: 8px; height: 8px; border-radius: 2px; }
header b { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-weight: 600; }
header button { border: 0; background: none; color: var(--dt-dim); font-size: 16px; cursor: default; }
header button:hover { color: var(--dt-ink); }
.body { flex: 1; min-height: 0; overflow-y: auto; }
</style>
