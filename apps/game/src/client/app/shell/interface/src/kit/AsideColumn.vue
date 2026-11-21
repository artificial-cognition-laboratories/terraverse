<script setup lang="ts">
/**
 * A page's content centred in a column, with an optional aside to its left that stays
 * put while the content scrolls: filters beside results. The whole stage scrolls as one.
 */
withDefaults(defineProps<{ width?: number; aside?: number }>(), { width: 600, aside: 220 })
</script>

<template>
    <div class="aside-column" :style="{ '--width': `${width}px`, '--aside': `${aside}px` }">
        <div class="frame" :class="{ sided: $slots.aside }">
            <aside v-if="$slots.aside"><slot name="aside" /></aside>
            <main><slot /></main>
        </div>
    </div>
</template>

<style scoped>
/* both gutters kept, so the column stays centred and nothing shifts when the scrollbar comes and goes */
.aside-column { height: 100%; min-height: 0; overflow-y: auto; scrollbar-gutter: stable both-edges; }
.frame { display: grid; grid-template-columns: minmax(0, var(--width)); justify-content: center; gap: 32px; padding: 22px 26px 40px; }
.frame.sided { grid-template-columns: var(--aside) minmax(0, var(--width)); }
/* the aside centres with the content, so the pair reads as one block */
/* stuck at the frame's own top padding, so it doesn't move at all as the page scrolls */
aside { position: sticky; top: 22px; align-self: start; display: grid; gap: 20px; }
main { display: flex; flex-direction: column; gap: 14px; min-width: 0; }
@media (max-width: 950px) {
    .frame.sided { grid-template-columns: minmax(0, var(--width)); }
    aside { position: static; max-height: none; }
}
</style>
