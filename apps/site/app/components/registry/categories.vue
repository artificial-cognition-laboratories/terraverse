<script setup lang="ts">
import { ICONS } from "~/config/icons"
import type { Category } from "~/registry"

/** Every category as a glass tile: its icon, its name and how many there are. Each opens Browse on it. */
defineProps<{ categories: readonly Category[] }>()
</script>

<template>
    <div class="categories">
        <NuxtLink
            v-for="c in categories"
            :key="c.id"
            :to="{ path: '/registry/browse', query: { category: c.id } }"
            class="category"
            :class="{ none: !c.count }"
        >
            <home-pixel-icon :icon="ICONS[c.id]" />
            <span class="name">{{ c.label }}</span>
            <span class="count">{{ c.count }}</span>
        </NuxtLink>
    </div>
</template>

<style scoped lang="scss">
.categories {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 0.75rem;
}

.category {
    @include panel;
    display: flex;
    gap: 0.9rem;
    align-items: center;
    min-width: 0;
    padding: 0.85rem 1rem;
    text-decoration: none;
    transition: border-color 0.1s;

    &:hover {
        border-color: rgba($primary-ink, 0.55);

        .name {
            color: $ink;
        }
    }
}

.name {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    font-family: $font-display;
    font-size: 0.68rem;
    text-transform: uppercase;
    text-overflow: ellipsis;
    white-space: nowrap;
    color: $ink-dim;
}

.none {
    .name,
    .count {
        color: $ink-faint;
    }

    :deep(svg),
    :deep(img) {
        opacity: 0.45;
    }
}

.count {
    font-size: 0.68rem;
    color: $ink-faint;
}

@include desktop-down {
    .categories {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}
</style>
