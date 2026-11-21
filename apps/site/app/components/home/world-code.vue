<script setup lang="ts">
/** A world's code, offered to copy: "this world is TF24…". */
defineProps<{ code: string }>()
const copy = useCopy()
</script>

<template>
    <button class="code" type="button" :title="`Copy ${code}`" @click="copy.copy(code)">
        <span class="mark" aria-hidden="true">»</span>
        <span>this world is</span>
        <code>{{ code }}</code>
        <span class="state">{{ copy.label.value }}</span>
    </button>
</template>

<style scoped lang="scss">
.code {
    display: inline-flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.45rem 0.8rem;
    font-family: $font-mono;
    font-size: 0.72rem;
    color: $ink-dim;
    background: rgba($night-0, 0.7);
    border: 1px solid rgba($ink, 0.08);
    cursor: pointer;

    code {
        color: $ink;
    }

    .mark {
        color: $primary-ink;
    }

    .state {
        min-width: 4.5em;
        color: $ink-faint;
        text-align: right;
    }

    &:hover .state {
        color: $primary-ink;
    }
}
</style>
