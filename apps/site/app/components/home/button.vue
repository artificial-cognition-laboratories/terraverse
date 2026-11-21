<script setup lang="ts">
import type { RouteLocationRaw } from "vue-router"

/**
 * The game's button: a raised block, lit top-left. A link with `to`, a button
 * without (it emits click). `primary` is the deep ocean, for what a page wants
 * done; `ghost` is quiet stone.
 */
withDefaults(
    defineProps<{ to?: RouteLocationRaw; variant?: "primary" | "ghost"; size?: "md" | "lg"; disabled?: boolean; type?: "button" | "submit" }>(),
    { to: undefined, variant: "ghost", size: "md", disabled: false, type: "button" },
)
const emit = defineEmits<{ click: [] }>()
</script>

<template>
    <NuxtLink v-if="to" :to="to" class="button" :class="[variant, size]">
        <slot />
    </NuxtLink>
    <button v-else :type="type" class="button" :class="[variant, size]" :disabled="disabled" @click="emit('click')">
        <slot />
    </button>
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
    cursor: pointer;
    transition: background 0.1s, color 0.1s;

    &:disabled {
        cursor: default;
        opacity: 0.5;
    }

    &:active:not(:disabled) {
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

    &:hover:not(:disabled) {
        background: color.adjust($primary, $lightness: 6%);
    }
}

.ghost {
    background: $night-2;
    color: $ink;

    &:hover:not(:disabled) {
        background: $night-3;
    }
}
</style>
