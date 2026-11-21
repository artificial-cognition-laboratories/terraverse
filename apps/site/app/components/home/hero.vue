<script setup lang="ts">
import { hero, links } from "~/config/home"
import { horizon } from "~/lib/cover"

// One fixed world, so the prerendered page and the hydrated one draw the same
const art = horizon(hero.code, 360, 150)
const code = useCopy()
</script>

<template>
    <section class="hero">
        <home-art class="sky" :art="art" anchor="bottom" />

        <div class="inner">
            <p class="kicker">{{ hero.kicker }}</p>
            <h1>{{ hero.title[0] }}<br /><em>{{ hero.title[1] }}</em></h1>
            <p class="lede">{{ hero.lede }}</p>

            <home-download-tiles />
            <div class="actions">
                <home-button :to="links.registry" variant="primary">Browse the registry »</home-button>
                <home-button :to="links.docs">Read the docs</home-button>
            </div>
        </div>

        <button class="code" type="button" :title="`Copy ${hero.code}`" @click="code.copy(hero.code)">
            <span class="mark" aria-hidden="true">»</span>
            <span>this world is</span>
            <code>{{ hero.code }}</code>
            <span class="state">{{ code.label.value }}</span>
        </button>
    </section>
</template>

<style scoped lang="scss">
.hero {
    position: relative;
    display: flex;
    align-items: center;
    min-height: clamp(640px, 100dvh, 920px);
    // the land fills the foot of the scene: the words stay in the sky
    padding: 52px 0 14rem;
    overflow: hidden;
}

.sky {
    position: absolute;
    inset: 0;
}

.inner {
    position: relative;
    width: 100%;
    max-width: $site-width;
    margin: 0 auto;
    padding: 0 2rem;
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
    margin: 1.25rem 0 1.5rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: clamp(2.2rem, 5vw, 3.75rem);
    line-height: 1.05;
    color: $ink;
    text-shadow: 0 4px 0 $edge;

    em {
        font-style: normal;
        color: $primary-ink;
    }
}

.lede {
    max-width: 34rem;
    margin: 0 0 2.25rem;
    font-family: $font-sans;
    font-size: 1.1rem;
    line-height: 1.65;
    color: $ink-dim;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    margin-top: 1.25rem;
}

// Sits on the land, like a caption on a photograph
.code {
    position: absolute;
    right: max(2rem, calc((100% - #{$site-width}) / 2 + 2rem));
    bottom: 1.5rem;
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.45rem 0.8rem;
    font-family: $font-mono;
    font-size: 0.72rem;
    color: $ink-dim;
    background: rgba($stone-0, 0.7);
    border: 1px solid rgba($ink, 0.08);
    cursor: pointer;

    code {
        color: $ink;
    }

    .mark {
        color: $primary-ink;
    }

    .state {
        min-width: 4.5em;
        color: $ink-faint;
        text-align: right;
    }

    &:hover .state {
        color: $primary-ink;
    }
}

@include mobile {
    .hero {
        padding-bottom: 12rem;
    }

    .inner {
        padding: 0 1.25rem;
    }

    .actions > * {
        width: 100%;
        justify-content: center;
    }

    .code {
        left: 1.25rem;
        right: auto;
    }
}
</style>
