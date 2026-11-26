<script setup lang="ts">
/** Grid or list: two joined square buttons, the chosen one lit. */
const model = defineModel<"grid" | "list">({ required: true })
</script>

<template>
    <div class="layout-toggle" role="group" aria-label="Layout">
        <button type="button" :class="{ on: model === 'grid' }" aria-label="Grid" @click="model = 'grid'">▦</button>
        <button type="button" :class="{ on: model === 'list' }" aria-label="List" @click="model = 'list'">≡</button>
    </div>
</template>

<style scoped>
.layout-toggle { display: flex; }
button { width: 2.1rem; height: 2.1rem; padding: 0; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); color: var(--ink-faint); font-size: 0.95rem; }
button + button { border-left: none; }
button:hover { color: var(--ink); }
button.on { color: var(--ink); background: rgb(10 75 145 / 0.3); border-color: var(--edge-lit); }
</style>
