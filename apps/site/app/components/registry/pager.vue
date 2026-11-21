<script setup lang="ts">
/** Previous, "n of m", next: emits the page wanted. */
const props = defineProps<{ page: number; pages: number }>()
defineEmits<{ go: [page: number] }>()
const last = computed(() => Math.max(1, props.pages))
</script>

<template>
    <nav class="pager" aria-label="Pages">
        <button type="button" :disabled="page <= 1" aria-label="Previous page" @click="$emit('go', page - 1)">‹</button>
        <span><strong>{{ page }}</strong> of {{ last }}</span>
        <button type="button" :disabled="page >= last" aria-label="Next page" @click="$emit('go', page + 1)">›</button>
    </nav>
</template>

<style scoped lang="scss">
.pager {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    font-size: 0.75rem;
    color: $ink-faint;

    strong {
        color: $ink;
    }
}

button {
    @include panel;
    width: 2rem;
    height: 2rem;
    font-size: 1rem;
    color: $ink;
    cursor: pointer;

    &:hover:not(:disabled) {
        border-color: rgba($primary-ink, 0.55);
    }

    &:disabled {
        color: $ink-faint;
        cursor: default;
    }
}
</style>
