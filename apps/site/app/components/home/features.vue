<template>
    <section class="features">
        <NuxtLink v-for="f in features" :key="f.kicker" :to="f.to" class="feature">
            <span class="kicker">» {{ f.kicker }}</span>
            <h2>{{ f.title }}</h2>
            <p>{{ f.body }}</p>
        </NuxtLink>
    </section>
</template>

<script setup lang="ts">
import { features } from "~/config/features"
</script>

<style scoped lang="scss">
.features {
    max-width: $site-width;
    margin: 0 auto;
    padding: 6rem 2rem;
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1.25rem;
}

.feature {
    display: block;
    min-width: 0;
    padding: 1.75rem;
    border: 1px solid $border-soft;
    text-decoration: none;

    &:hover {
        border-color: $border-strong;
        background: rgba($primary, 0.03);
    }
}

.kicker {
    font-size: 0.72rem;
    color: $primary;
    text-transform: uppercase;
    letter-spacing: 0.08em;
}

h2 {
    font-family: $font-display;
    font-weight: 400;
    font-size: 1.35rem;
    color: $text-primary;
    margin: 0.9rem 0 0.75rem;
}

p {
    margin: 0;
    color: $text-secondary;
    font-size: 0.85rem;
    line-height: 1.7;
}

@include tablet {
    .features {
        grid-template-columns: minmax(0, 1fr);
        padding: 4rem 1.25rem;
    }
}
</style>
