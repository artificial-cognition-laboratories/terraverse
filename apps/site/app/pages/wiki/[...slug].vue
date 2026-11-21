<script setup lang="ts">
import { hero } from "~/config/home"

// TEMP(2026-11-21): the wiki is coming. Its pages wait in content/wiki, and its
// nav in config/docs-nav (wikiSections); until it's back every /wiki address
// lands here, and the topnav leaves it out (config/site.ts).
definePageMeta({ layout: "landing" })
useHead({ title: "Wiki" })
</script>

<template>
    <div class="wiki">
        <home-sky :seed="hero.code" scene="stars" />
        <section class="soon">
            <p class="kicker">» The wiki</p>
            <h1>Coming soon.</h1>
            <p class="lede">
                Every block, substance and reaction, explained by the game's own scenes running the real code. Until it's
                here, the docs cover building on Terraverse.
            </p>
            <div class="actions">
                <home-button to="/docs" variant="primary">Read the docs</home-button>
                <home-button to="/registry">Browse the registry</home-button>
            </div>
        </section>
    </div>
</template>

<style scoped lang="scss">
.wiki {
    position: relative;
    isolation: isolate;
    display: grid;
    place-items: center;
    min-height: 100dvh;
    padding: calc(#{$topnav-height} + 4rem) 1.5rem 6rem;
}

.soon {
    @include on-sky;
    display: grid;
    justify-items: center;
    gap: 1.1rem;
    max-width: 36rem;
    text-align: center;
}

.kicker {
    margin: 0;
    font-family: $font-display;
    font-size: 0.65rem;
    text-transform: uppercase;
    color: $primary-ink;
}

h1 {
    margin: 0;
    font-family: $font-sans;
    font-weight: 600;
    font-size: clamp(2rem, 5vw, 3rem);
    color: $ink;
}

.lede {
    margin: 0;
    font-family: $font-sans;
    line-height: 1.7;
    color: $ink-dim;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 0.75rem;
    margin-top: 0.75rem;
}
</style>
