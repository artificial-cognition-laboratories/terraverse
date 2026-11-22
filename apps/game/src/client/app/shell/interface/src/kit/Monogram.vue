<script setup lang="ts">
import { computed } from "vue"

/** A package's mark: its icon, or the first letters of its name in a framed square. */
const props = withDefaults(defineProps<{ name: string; image?: string | null; size?: number }>(), { image: null, size: 40 })
const letters = computed(() => {
    const bare = props.name.replace(/^@[^/]+\//, "")
    const words = bare.split(/[-_\s.]+/).filter(Boolean)
    const [first, second] = words
    return (first && second ? first.charAt(0) + second.charAt(0) : bare.slice(0, 2)).toUpperCase()
})
</script>

<template>
    <span class="monogram" :style="{ '--s': `${size}px` }">
        <img v-if="image" :src="image" alt="" draggable="false">
        <template v-else>{{ letters }}</template>
    </span>
</template>

<style scoped>
.monogram { display: grid; place-items: center; flex: none; width: var(--s); height: var(--s); overflow: hidden; border: 1px solid var(--glass-edge); background: rgb(4 7 11 / 0.85); color: var(--accent-hi); font: calc(var(--s) * 0.27) var(--font-display); }
img { width: 100%; height: 100%; object-fit: cover; image-rendering: pixelated; }
</style>
