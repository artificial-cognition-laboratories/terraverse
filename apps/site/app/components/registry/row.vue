<script setup lang="ts">
import { CATEGORY_LABELS, TYPE_LABELS, type Listing } from "~/registry"
import { cover } from "~/lib/cover"
import { format } from "~/lib/format"

/** A listing as one row of a list: its picture, name, words and numbers side by side. */
const props = defineProps<{ listing: Listing }>()
const art = computed(() => cover(props.listing.name, 24))
</script>

<template>
    <NuxtLink :to="`/registry/${listing.name}`" class="row">
        <div class="picture">
            <home-art class="art" :art="art" />
            <span v-if="listing.official" class="badge">Official</span>
        </div>
        <div class="body">
            <div class="head">
                <registry-monogram :title="listing.title" size="sm" />
                <div class="names">
                    <h3>{{ listing.title }}</h3>
                    <span class="owner">{{ listing.name }}</span>
                </div>
            </div>
            <p>{{ listing.description }}</p>
            <div class="meta">
                <span>★ {{ format.count(listing.stars) }}</span>
                <span>↓ {{ format.count(listing.installs) }}</span>
                <span>v{{ listing.latestVersion }}</span>
                <span>{{ format.date(listing.updatedAt) }}</span>
                <span class="tags">
                    <span class="tag">{{ TYPE_LABELS[listing.type] }}</span>
                    <span v-for="c in listing.categories" :key="c" class="tag">{{ CATEGORY_LABELS[c] }}</span>
                </span>
            </div>
        </div>
    </NuxtLink>
</template>

<style scoped lang="scss">
.row {
    @include panel;
    display: flex;
    min-width: 0;
    text-decoration: none;
    transition: border-color 0.1s;

    &:hover {
        border-color: rgba($primary-ink, 0.55);

        h3 {
            color: $primary-ink;
        }
    }
}

.picture {
    position: relative;
    flex: none;
    width: 11rem;
}

.art {
    height: 100%;
    border-right: 1px solid $glass-edge;
}

.badge {
    position: absolute;
    top: 0.5rem;
    left: 0.5rem;
    padding: 0.15rem 0.35rem;
    font-family: $font-display;
    font-size: 0.5rem;
    text-transform: uppercase;
    color: $primary-ink;
    background: rgba($night-sky, 0.85);
    border: 1px solid rgba($primary-ink, 0.55);
}

.body {
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
    flex: 1;
    min-width: 0;
    padding: 1rem 1.2rem;
}

.head {
    display: flex;
    gap: 0.75rem;
    align-items: center;
}

.names {
    display: grid;
    min-width: 0;
}

h3 {
    margin: 0;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.8rem;
    text-transform: uppercase;
    color: $ink;
}

.owner {
    font-size: 0.68rem;
    color: $ink-faint;
}

p {
    margin: 0;
    overflow: hidden;
    font-family: $font-sans;
    font-size: 0.88rem;
    color: $ink-dim;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.meta {
    display: flex;
    flex-wrap: wrap;
    gap: 0.4rem 1rem;
    align-items: center;
    font-size: 0.68rem;
    color: $ink-faint;
}

.tags {
    display: flex;
    gap: 0.4rem;
    margin-left: auto;
}

.tag {
    padding: 0.1rem 0.4rem;
    border: 1px solid $glass-edge;
}

@include mobile {
    .picture {
        display: none;
    }

    .tags {
        margin-left: 0;
    }
}
</style>
