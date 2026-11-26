<script setup lang="ts">
import { facts, hero, links } from "~/config/home"
</script>

<template>
    <section class="hero">
        <div class="inner">
            <p class="kicker">{{ hero.kicker }}</p>
            <h1>{{ hero.title[0] }}<br /><em>{{ hero.title[1] }}</em></h1>
            <p class="lede">{{ hero.lede }}</p>

            <!-- getting it: the downloads and the install line under them, as one group -->
            <div class="get">
                <home-download />
            </div>
            <div class="actions">
                <home-button :to="links.registry" variant="primary">Browse the registry »</home-button>
                <home-button :to="links.docs">Read the docs</home-button>
            </div>
            <ul class="facts">
                <li v-for="fact in facts" :key="fact">{{ fact }}</li>
            </ul>
        </div>

    </section>
</template>

<style scoped lang="scss">
// Centred, the words and downloads on the axis between the sun and the moon
.hero {
    position: relative;
    display: flex;
    align-items: center;
    text-align: center;
    min-height: clamp(700px, 96dvh, 960px);
    padding: calc(52px + 5rem) 0 6rem;
    overflow: hidden;
}

.inner {
    @include on-sky;
    position: relative;
    width: 100%;
    max-width: $site-width;
    margin: 0 auto;
    padding: 0 2rem;

    :deep(.tiles) {
        justify-content: center;
    }
}

.get {
    display: inline-block;
    max-width: 100%;
}

.kicker {
    margin: 0;
    font-family: $font-display;
    font-size: 0.75rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: $primary-ink;
}

h1 {
    margin: 1.5rem 0 1.75rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: clamp(2.4rem, 7vw, 5.25rem);
    line-height: 1.05;
    color: $ink;
    text-shadow: 0 4px 0 $edge;

    em {
        font-style: normal;
        color: $primary-ink;
    }
}

.lede {
    max-width: 37rem;
    margin: 0 auto 2.5rem;
    font-family: $font-sans;
    font-size: 1.1rem;
    line-height: 1.65;
    color: $ink-dim;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 0.75rem;
    margin-top: 1.75rem;
}

.facts {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 0.5rem 1.6rem;
    margin: 2.5rem 0 0;
    padding: 0;
    list-style: none;

    li {
        font-family: $font-display;
        font-size: 0.62rem;
        text-transform: uppercase;
        letter-spacing: 0.06em;
        color: $ink-faint;
        white-space: nowrap;

        &::before {
            content: "■";
            margin-right: 0.5rem;
            color: $primary-ink;
            font-size: 0.5rem;
            vertical-align: 0.1em;
        }
    }
}

@include mobile {
    .hero {
        padding-top: calc(52px + 2.5rem);
    }

    .inner {
        padding: 0 1.25rem;
    }

    .get {
        display: block;
    }

    .actions > * {
        width: 100%;
        justify-content: center;
    }
}
</style>
