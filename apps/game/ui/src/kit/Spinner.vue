<script setup lang="ts">
/**
 * Something under way for no time anyone can say: four blocks taking turns
 * round a square, stepped (a few repaints a second, never a smooth spin).
 */
defineProps<{ label?: string }>()
</script>

<template>
    <span class="spinner" role="status" :aria-label="label ?? 'Working'">
        <i /><i /><i /><i />
    </span>
</template>

<style scoped>
.spinner { display: inline-grid; grid-template-columns: repeat(2, 5px); gap: 2px; flex: none; }
i { width: 5px; height: 5px; background: currentColor; opacity: 0.25; animation: turn 0.8s steps(1, end) infinite; }
/* clockwise round the square: top left, top right, bottom right, bottom left */
i:nth-child(1) { animation-delay: 0s; }
i:nth-child(2) { animation-delay: 0.2s; }
i:nth-child(4) { animation-delay: 0.4s; }
i:nth-child(3) { animation-delay: 0.6s; }
@keyframes turn {
    0% { opacity: 1; }
    25% { opacity: 0.25; }
}
</style>
