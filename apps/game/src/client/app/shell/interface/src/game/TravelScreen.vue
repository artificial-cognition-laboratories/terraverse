<script setup lang="ts">
import { computed } from "vue"
import { useStore } from "./store.ts"

/** /travel: while the land on another body is made, the world behind is hidden (server/planet.cpp). */
const store = useStore()
const travel = computed(() => store.ui.travel)
</script>

<template>
    <div v-if="travel" class="travel">
        <p class="to">Travelling to {{ travel.name }}</p>
        <p class="hint">making the land there</p>
    </div>
</template>

<style scoped>
.travel {
    position: fixed;
    inset: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 8px;
    background: #05070c;
    color: #e8ecf4;
    pointer-events: all;
}

.to {
    margin: 0;
    font-size: 28px;
    letter-spacing: 0.04em;
}

.hint {
    margin: 0;
    font-size: 14px;
    opacity: 0.6;
}
</style>
