<script setup lang="ts">
/** A callout: something the player should know about the screen they're on. */
withDefaults(defineProps<{ tone?: "info" | "warn" | "danger" }>(), { tone: "info" })
</script>

<template>
    <p class="notice" :class="tone"><slot /></p>
</template>

<style scoped>
.notice { margin: 0; padding: 10px 12px; border-left: 4px solid; font-size: 12px; color: var(--ink-dim); }
.notice :deep(strong) { color: var(--ink); }
.info { border-color: var(--accent); background: rgb(0 148 210 / 0.07); }
.warn { border-color: #d9a441; background: rgb(217 164 65 / 0.08); }
.danger { border-color: var(--danger); background: rgb(224 83 61 / 0.1); }
</style>
