<script setup lang="ts">
/**
 * The bar under the topnav on the registry and the docs: where you can go in
 * this part of the site on the left, its search on the right (the slot). It
 * stays under the topnav as the page scrolls.
 */
defineProps<{ links: readonly { label: string; to: string; on: boolean }[] }>()
</script>

<template>
    <div class="subnav">
        <div class="inner">
            <nav class="links">
                <NuxtLink v-for="l in links" :key="l.to" :to="l.to" class="link" :class="{ on: l.on }">
                    {{ l.label }}
                </NuxtLink>
            </nav>
            <slot />
        </div>
    </div>
</template>

<style scoped lang="scss">
// Exactly $subnav-height, its border included, so what sits under it can count on it
.subnav {
    box-sizing: border-box;
    height: $subnav-height;
    position: sticky;
    top: $topnav-height;
    z-index: 900;
    border-bottom: 1px solid $glass-edge;
    background: rgba($night-sky, 0.72);
    backdrop-filter: blur(10px);
}

.inner {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 2rem;
    max-width: $site-width;
    height: 100%;
    margin: 0 auto;
    padding: 0 2rem;
}

.links {
    display: flex;
    align-self: stretch;
    gap: 1.75rem;
    min-width: 0;
    overflow-x: auto;
    scrollbar-width: none;
}

.link {
    display: flex;
    flex: none;
    align-items: center;
    font-family: $font-display;
    font-size: 0.68rem;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    color: $ink-faint;
    text-decoration: none;
    white-space: nowrap;
    border-bottom: 1px solid transparent;

    &:hover {
        color: $ink;
    }

    &.on {
        color: $primary-ink;
        border-bottom-color: $primary-ink;
    }
}

@include mobile {
    .inner {
        padding: 0 1.25rem;
        gap: 1rem;
    }
}
</style>
