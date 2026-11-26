<script setup lang="ts">
import type { Owner } from "~/registry"
import { format } from "~/lib/format"

/** The top of an owner's page, as a mod's: their banner, monogram, name and numbers. */
const props = defineProps<{ owner: Owner }>()
const name = computed(() => props.owner.scope.slice(1))
</script>

<template>
    <header class="header">
        <registry-banner :seed="owner.scope" />
        <div class="title">
            <registry-monogram :title="name" size="lg" />
            <div class="names">
                <div class="line">
                    <h1>{{ owner.scope }}</h1>
                    <span v-if="owner.official" class="badge official">Official</span>
                </div>
                <span class="by">
                    {{ owner.listings.length }} published · ★ {{ format.count(owner.stars) }} · ↓ {{ format.count(owner.installs) }}
                </span>
            </div>
        </div>
    </header>
</template>

<style scoped lang="scss">
.header {
    @include on-sky;
    display: grid;
    gap: 1.25rem;
}

.title {
    display: flex;
    gap: 1.1rem;
    align-items: center;
    margin-top: -3.25rem;
    padding: 0 1.25rem;
    position: relative;
}

.names {
    flex: 1;
    display: grid;
    gap: 0.35rem;
    min-width: 0;
    padding-top: 2.5rem;
}

.line {
    min-width: 0;
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    align-items: center;
}

// a long name (one word, as a scope is) wraps rather than pushing the page wider than a phone
h1 {
    min-width: 0;
    overflow-wrap: anywhere;
    margin: 0;
    overflow: hidden;
    font-family: $font-display;
    font-weight: 400;
    font-size: clamp(1.4rem, 3vw, 2rem);
    text-transform: uppercase;
    text-overflow: ellipsis;
    color: $ink;
}

.badge.official {
    padding: 0.2rem 0.45rem;
    font-family: $font-display;
    font-size: 0.55rem;
    text-transform: uppercase;
    color: $primary-ink;
    border: 1px solid rgba($primary-ink, 0.55);
    border-radius: $radius;
}

.by {
    font-size: 0.78rem;
    color: $ink-faint;
}
</style>
