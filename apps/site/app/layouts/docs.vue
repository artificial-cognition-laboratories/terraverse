<script setup lang="ts">
import { hero } from "~/config/home"

/** Every docs page: the night with its stars, as the registry, under the docs' subnav; the tree beside the article. */
const nav = useDocsNav()
</script>

<template>
    <div class="docs-layout">
        <shared-topnav />
        <home-sky :seed="hero.code" scene="quiet" />
        <docs-subnav />
        <docs-where />

        <div class="docs-container">
            <docs-sidebar :section="nav.activeSection.value" />
            <main class="docs-main">
                <slot />
            </main>
        </div>
    </div>
</template>

<style scoped lang="scss">
.docs-layout {
    position: relative;
    isolation: isolate;
    min-height: 100dvh;
    padding-top: $topnav-height;
}

.docs-container {
    display: grid;
    grid-template-columns: $side-width minmax(0, 1fr);
    gap: 3rem;
    align-items: start;
    max-width: $site-width;
    margin: 0 auto;
    padding: $page-top 2rem 0;
}

// The room under the article is the article's, not the container's: the sticky tree sticks
// until the container's content ends, so padding there pushed it up off its place at the bottom
.docs-main {
    min-width: 0;
    padding-bottom: 6rem;
}

@include tablet {
    .docs-container {
        grid-template-columns: minmax(0, 1fr);
        padding: 0 1.25rem;
    }
}

@include mobile {
    .docs-container {
        padding: 0 1rem;
    }
}
</style>
