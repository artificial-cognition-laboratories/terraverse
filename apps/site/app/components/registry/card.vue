<script setup lang="ts">
import { CATEGORY_LABELS, type Listing } from "~/registry"
import { cover } from "~/lib/cover"
import { format } from "~/lib/format"

/** A listing as a card: its picture, monogram, name, owner and numbers. It links to its page. */
const props = defineProps<{ listing: Listing }>()
const art = computed(() => cover(props.listing.name, 32))
</script>

<template>
    <NuxtLink :to="`/registry/${listing.name}`" class="card">
        <div class="picture">
            <home-art class="art" :art="art" />
            <span v-if="listing.official" class="badge">Official</span>
        </div>
        <div class="head">
            <registry-monogram :title="listing.title" />
            <div class="names">
                <h3>{{ listing.title }}</h3>
                <span class="owner">{{ listing.owner }}</span>
            </div>
        </div>
        <p>{{ listing.description }}</p>
        <div class="meta">
            <span>★ {{ format.count(listing.stars) }}</span>
            <span>↓ {{ format.count(listing.installs) }}</span>
            <span v-if="listing.categories[0]" class="tag">{{ CATEGORY_LABELS[listing.categories[0]] }}</span>
        </div>
    </NuxtLink>
</template>

<style scoped lang="scss">
.card {
    display: flex;
    flex-direction: column;
    min-width: 0;
    @include panel;
    overflow: hidden; // the picture's corners follow the card's
    text-decoration: none;
    transition: background 0.1s;

    &:hover {
        border-color: rgba($primary-ink, 0.55);

        h3 {
            color: $primary-ink;
        }
    }
}

.picture {
    position: relative;
}

.art {
    aspect-ratio: 16 / 9;
    border-bottom: 1px solid $glass-edge;
}

.badge {
    position: absolute;
    top: 0.6rem;
    left: 0.6rem;
    padding: 0.2rem 0.4rem;
    font-family: $font-display;
    font-size: 0.55rem;
    text-transform: uppercase;
    color: $primary-ink;
    background: rgba($night-0, 0.85);
    border: 1px solid $primary-ink;
    border-radius: $radius;
}

.head {
    display: flex;
    gap: 0.75rem;
    align-items: center;
    margin: -1.3rem 0.9rem 0;
    position: relative;
}


.names {
    display: grid;
    min-width: 0;
    padding-top: 1.3rem;
}

h3 {
    margin: 0;
    overflow: hidden;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.78rem;
    text-transform: uppercase;
    text-overflow: ellipsis;
    white-space: nowrap;
    color: $ink;
}

.owner {
    font-size: 0.68rem;
    color: $ink-faint;
}

p {
    flex: 1;
    margin: 0.8rem 0.9rem 0;
    font-family: $font-sans;
    font-size: 0.82rem;
    line-height: 1.5;
    color: $ink-dim;
    display: -webkit-box;
    -webkit-line-clamp: 2;
    -webkit-box-orient: vertical;
    overflow: hidden;
}

.meta {
    white-space: nowrap;
    display: flex;
    gap: 0.9rem;
    align-items: center;
    padding: 0.9rem;
    font-size: 0.68rem;
    color: $ink-faint;
}

.tag {
    margin-left: auto;
    padding: 0.1rem 0.4rem;
    border: 1px solid $glass-edge;
    border-radius: $radius;
}
</style>
