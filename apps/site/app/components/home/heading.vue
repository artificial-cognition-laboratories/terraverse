<script setup lang="ts">
/** A section's head: the kicker, the claim, a line under it, and an optional link on the right. */
defineProps<{ kicker: string; title: string; lede?: string; link?: { label: string; to: string } }>()
</script>

<template>
    <header class="heading">
        <div class="words">
            <p class="kicker"><span aria-hidden="true">»</span> {{ kicker }}</p>
            <h2>{{ title }}</h2>
            <p v-if="lede" class="lede">{{ lede }}</p>
        </div>
        <NuxtLink v-if="link" :to="link.to" class="more">{{ link.label }} »</NuxtLink>
    </header>
</template>

<style scoped lang="scss">
.heading {
    @include on-sky;
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 2rem;
    margin-bottom: 2.5rem;
}

.words {
    max-width: 40rem;
}

.kicker {
    margin: 0 0 0.9rem;
    font-family: $font-display;
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $primary-ink;
}

h2 {
    margin: 0;
    font-family: $font-sans;
    font-weight: 700;
    font-size: clamp(1.6rem, 3vw, 2.25rem);
    line-height: 1.15;
    letter-spacing: -0.01em;
    color: $ink;
}

.lede {
    margin: 1rem 0 0;
    font-family: $font-sans;
    font-size: 1rem;
    line-height: 1.65;
    color: $ink-dim;
}

.more {
    flex: none;
    font-family: $font-display;
    font-size: 0.7rem;
    text-transform: uppercase;
    color: $primary-ink;
    text-decoration: none;
    white-space: nowrap;

    &:hover {
        color: $ink;
    }
}

@include mobile {
    .heading {
        flex-direction: column;
        align-items: flex-start;
        gap: 1rem;
    }
}
</style>
