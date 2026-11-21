<script setup lang="ts">
/**
 * A link that looks like the game's buttons: a raised block, lit top-left.
 * `primary` is the deep ocean, for what a section wants done; `ghost` is quiet stone.
 */
withDefaults(defineProps<{ to: string; variant?: "primary" | "ghost"; size?: "md" | "lg" }>(), {
    variant: "ghost",
    size: "md",
})
</script>

<template>
    <NuxtLink :to="to" class="button" :class="[variant, size]">
        <slot />
    </NuxtLink>
</template>

<style scoped lang="scss">
@use "sass:color";

.button {
    display: inline-flex;
    align-items: center;
    gap: 0.6rem;
    height: 2.6rem;
    padding: 0 1.2rem;
    font-family: $font-display;
    font-size: 0.78rem;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    white-space: nowrap;
    text-decoration: none;
    border: 1px solid $edge;
    box-shadow: $raised;
    transition: background 0.1s, color 0.1s;

    &:active {
        box-shadow: $sunk;
        transform: translateY($bevel);
    }

    &.lg {
        height: 3.1rem;
        padding: 0 1.6rem;
        font-size: 0.88rem;
    }
}

.primary {
    background: $primary;
    color: #fff;

    &:hover {
        background: color.adjust($primary, $lightness: 6%);
    }
}

.ghost {
    background: $stone-2;
    color: $ink;

    &:hover {
        background: $stone-3;
    }
}
</style>
