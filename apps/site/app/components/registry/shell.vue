<script setup lang="ts">
import { hero } from "~/config/home"

/** Every registry page's frame: the night with its stars, the subnav, and the page held to the site's measure. */
</script>

<template>
    <div class="shell">
        <home-sky :seed="hero.code" scene="stars" />
        <registry-subnav />
        <div class="page">
            <slot />
        </div>
    </div>
</template>

<style scoped lang="scss">
.shell {
    position: relative;
    isolation: isolate;
    min-height: 100dvh;
    padding: 52px 0 6rem;
}

.page {
    max-width: $site-width;
    margin: 0 auto;
    padding: 2.5rem 2rem 0;
}

@include mobile {
    .page {
        padding: 2rem 1.25rem 0;
    }
}
</style>
