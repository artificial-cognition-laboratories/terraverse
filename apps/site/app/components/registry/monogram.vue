<script setup lang="ts">
import { format } from "~/lib/format"

/** A listing's mark until it has an icon: two letters in a small glass square. */
withDefaults(defineProps<{ title: string; size?: "sm" | "md" | "lg" }>(), { size: "md" })
</script>

<template>
    <span class="monogram" :class="size" aria-hidden="true">{{ format.monogram(title) }}</span>
</template>

<style scoped lang="scss">
.monogram {
    @include well;
    flex: none;
    display: grid;
    place-items: center;
    font-family: $font-display;
    color: $primary-ink;
    background: rgba($night-sky, 0.85);

    &.sm {
        width: 2.2rem;
        height: 2.2rem;
        font-size: 0.65rem;
    }

    &.md {
        width: 2.8rem;
        height: 2.8rem;
        font-size: 0.78rem;
    }

    &.lg {
        width: 4rem;
        height: 4rem;
        font-size: 1.05rem;
    }
}
</style>
