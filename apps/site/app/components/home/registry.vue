<script setup lang="ts">
import { CATEGORY_ICONS } from "~/config/icons"
import { flags, links, registry, SAMPLE_LISTINGS, SAMPLE_SERVERS } from "~/config/home"

const featured = SAMPLE_LISTINGS.filter(l => l.official)
const trending = SAMPLE_LISTINGS.slice(0, 4)
</script>

<template>
    <home-band>
        <home-heading
            :kicker="registry.kicker"
            :title="registry.title"
            :lede="registry.lede"
            :link="{ label: 'Browse all', to: links.registry }"
        />

        <home-featured :listings="featured" :to="links.registry" />

        <h3 class="label">Categories</h3>
        <div class="categories">
            <NuxtLink v-for="c in registry.categories" :key="c.id" :to="links.registry" class="category">
                <home-pixel-icon :icon="CATEGORY_ICONS[c.id]" />
                <span>{{ c.label }}</span>
            </NuxtLink>
        </div>

        <div class="row-head">
            <h3 class="label">Trending</h3>
            <NuxtLink :to="links.registry" class="more">See all »</NuxtLink>
        </div>
        <div class="cards">
            <home-listing-card v-for="l in trending" :key="l.name" :listing="l" :to="links.registry" />
        </div>

        <template v-if="flags.servers">
            <div class="row-head">
                <h3 class="label">Live servers</h3>
                <NuxtLink :to="links.registry" class="more">All servers »</NuxtLink>
            </div>
            <home-server-list :servers="SAMPLE_SERVERS" />
        </template>
    </home-band>
</template>

<style scoped lang="scss">
.label {
    margin: 3rem 0 1rem;
    padding-bottom: 0.6rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $ink;
    border-bottom: 1px solid $stone-3;
}

.row-head {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    border-bottom: 1px solid $stone-3;
    margin: 3rem 0 1rem;

    .label {
        margin: 0;
        border: none;
    }
}

.more {
    font-family: $font-display;
    font-size: 0.62rem;
    text-transform: uppercase;
    color: $primary-ink;
    text-decoration: none;

    &:hover {
        color: $ink;
    }
}

.categories {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 0.75rem;
}

.category {
    display: flex;
    gap: 0.9rem;
    align-items: center;
    min-width: 0;
    padding: 0.85rem 1rem;
    font-family: $font-display;
    font-size: 0.68rem;
    text-transform: uppercase;
    color: $ink-dim;
    background: $stone-1;
    border: 1px solid $stone-3;
    text-decoration: none;
    white-space: nowrap;

    span {
        overflow: hidden;
        text-overflow: ellipsis;
    }

    &:hover {
        color: $ink;
        border-color: $primary-ink;
    }
}

.cards {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 1rem;
}

@include desktop-down {
    .categories,
    .cards {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@include mobile {
    .cards {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
