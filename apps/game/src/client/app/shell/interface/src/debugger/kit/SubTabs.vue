<script setup lang="ts" generic="T extends string">
/** The slim row of views inside a debugger tab (Live / Flame / Stats): the chosen one underlined. */
defineProps<{ tabs: readonly { id: T; label: string }[]; label: string }>()
const model = defineModel<T>({ required: true })
</script>

<template>
    <nav class="sub-tabs" role="tablist" :aria-label="label">
        <button v-for="t in tabs" :key="t.id" role="tab" :aria-selected="model === t.id" :class="{ on: model === t.id }" @click="model = t.id">
            {{ t.label }}
        </button>
    </nav>
</template>

<style scoped>
.sub-tabs { display: flex; align-self: stretch; flex: none; }
button { position: relative; display: flex; align-items: center; padding: 0 12px; border: 0; background: none; color: var(--dt-dim); font: inherit; font-size: 12px; white-space: nowrap; cursor: default; }
button:hover { color: var(--dt-ink); background: var(--dt-hover); }
button.on { color: var(--dt-ink); }
button.on::after { content: ""; position: absolute; left: 8px; right: 8px; bottom: 0; height: 2px; background: var(--dt-accent); }
</style>
