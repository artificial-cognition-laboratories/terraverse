<script setup lang="ts">
/** Highlighted code (Shiki's HTML, made at build) with line numbers, never scrolling the page sideways. */
defineProps<{ html: string }>()
</script>

<template>
    <!-- eslint-disable-next-line vue/no-v-html -- Shiki's output for our own code, made at build -->
    <div class="code" v-html="html" />
</template>

<style scoped lang="scss">
.code {
    padding: 1.25rem 0;
    overflow-x: auto;
    counter-reset: line;

    :deep(pre) {
        margin: 0;
        padding: 0;
        background: transparent !important;
        font-family: $font-mono;
        font-size: 0.82rem;
        line-height: 1.75;
    }

    :deep(code) {
        line-height: 1.75;
    }

    :deep(.line) {
        display: block;
        line-height: 1.75;
        counter-increment: line;

        &::before {
            content: counter(line);
            display: inline-block;
            width: 2rem;
            margin-right: 1.25rem;
            text-align: right;
            color: $night-4;
            user-select: none;
        }
    }
}
</style>
