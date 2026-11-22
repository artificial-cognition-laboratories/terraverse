<script setup lang="ts">
import { hero } from "~/config/home"

/**
 * An account screen: one glass card on the night, its title and words over
 * what the screen asks (the slot); small print under the card.
 */
defineProps<{ title: string; lede?: string }>()
</script>

<template>
    <div class="auth">
        <home-sky :seed="hero.code" scene="stars" />
        <section class="card">
            <div class="words">
                <h1>{{ title }}</h1>
                <p v-if="lede || $slots.lede" class="lede"><slot name="lede">{{ lede }}</slot></p>
            </div>
            <slot />
        </section>
        <p v-if="$slots.fine" class="fine"><slot name="fine" /></p>
    </div>
</template>

<style scoped lang="scss">
.auth {
    position: relative;
    isolation: isolate;
    display: grid;
    place-content: center;
    justify-items: center;
    gap: 1.25rem;
    min-height: 100dvh;
    grid-template-columns: minmax(0, 1fr);
    padding: calc(#{$topnav-height} + 3rem) 1.25rem 4rem;
}

// One column: nothing inside may widen it, so every child shrinks to fit
.card {
    @include panel;
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    gap: 2rem;
    width: min(27rem, 100%);
    padding: 2.75rem 2.25rem 2.5rem;
    background: rgba($night-0, 0.78);
}

.words {
    display: grid;
    gap: 0.6rem;
}

h1 {
    margin: 0;
    font-family: $font-sans;
    font-weight: 600;
    font-size: 1.75rem;
    color: $ink;
}

.lede {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.92rem;
    line-height: 1.6;
    color: $ink-dim;

    :deep(strong) {
        font-weight: 600;
        color: $ink;
    }
}

.fine {
    @include on-sky;
    max-width: 26rem;
    margin: 0;
    font-family: $font-sans;
    font-size: 0.78rem;
    line-height: 1.6;
    text-align: center;
    color: $ink-faint;

    :deep(a) {
        color: $primary-ink;
        text-decoration: none;
    }
}

@include mobile {
    .card {
        padding: 1.75rem 1.25rem 1.5rem;
    }
}
</style>
