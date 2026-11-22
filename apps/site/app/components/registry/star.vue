<script setup lang="ts">
import { useAccount } from "~/account"
import { ICONS } from "~/config/icons"
import { format } from "~/lib/format"

/**
 * Star a mod. Signed out, it's the way to sign in and come back; signed in,
 * it toggles. `stars` is everyone else's count: yours is added to it.
 */
const props = defineProps<{ name: string; stars: number }>()
const route = useRoute()
const account = useAccount()
const on = computed(() => account.stars.has(props.name))
const count = computed(() => props.stars + (on.value ? 1 : 0))

function press() {
    if (!account.user.value) return navigateTo({ path: "/signin", query: { next: route.fullPath } })
    account.stars.toggle(props.name)
}
</script>

<template>
    <button type="button" class="star" :class="{ on }" :aria-pressed="on" @click="press">
        <home-pixel-icon :icon="ICONS.star" :size="14" class="glyph" />
        <span class="label">{{ on ? "Starred" : "Star" }}</span>
        <span class="count">{{ format.count(count) }}</span>
    </button>
</template>

<style scoped lang="scss">
.star {
    @include panel;
    display: inline-flex;
    align-items: center;
    gap: 0.55rem;
    height: 2.1rem;
    padding: 0 0.75rem;
    font-family: $font-display;
    font-size: 0.62rem;
    text-transform: uppercase;
    color: $ink-dim;
    cursor: pointer;

    &:hover {
        color: $ink;
        border-color: rgba($primary-ink, 0.55);
    }

    // Unstarred the star is grey: it lights only when it's yours
    .glyph {
        filter: grayscale(1) brightness(0.8);
    }

    .count {
        padding-left: 0.55rem;
        font-family: $font-mono;
        font-size: 0.72rem;
        color: $ink-faint;
        border-left: 1px solid $glass-edge;
    }

    &.on {
        color: $ink;

        .glyph {
            filter: none;
        }
    }
}
</style>
