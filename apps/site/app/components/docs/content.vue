<template>
    <div class="docs-content">
        <article class="prose">
            <slot />
        </article>

        <nav v-if="prev || next" class="page-nav">
            <NuxtLink v-if="prev" :to="prev.path" class="page-nav-link prev">
                <span class="nav-label">« Previous</span>
                <span class="nav-title">{{ prev.title }}</span>
            </NuxtLink>
            <NuxtLink v-if="next" :to="next.path" class="page-nav-link next">
                <span class="nav-label">Next »</span>
                <span class="nav-title">{{ next.title }}</span>
            </NuxtLink>
        </nav>
    </div>
</template>

<script setup lang="ts">
import { areaOf, pagesOf } from "~/config/docs-nav"

const route = useRoute()
const pages = computed(() => pagesOf(areaOf(route.path)))
const index = computed(() => pages.value.findIndex(p => p.path === route.path))
const prev = computed(() => (index.value > 0 ? pages.value[index.value - 1] : undefined))
const next = computed(() =>
    index.value !== -1 && index.value < pages.value.length - 1 ? pages.value[index.value + 1] : undefined,
)
</script>

<style scoped lang="scss">
// The reading measure: the column is wider, the lines aren't
.docs-content {
    width: 100%;
    max-width: 48rem;
    min-width: 0;
}

.page-nav {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1rem;
    margin-top: 4rem;
}

.page-nav-link {
    @include panel;
    display: grid;
    gap: 0.4rem;
    padding: 1rem 1.25rem;
    text-decoration: none;

    &:hover {
        border-color: rgba($primary-ink, 0.55);
    }

    &.prev {
        grid-column: 1;
    }

    &.next {
        grid-column: 2;
        text-align: right;
    }
}

.nav-label {
    font-family: $font-display;
    font-size: 0.58rem;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    color: $ink-faint;
}

.nav-title {
    font-family: $font-sans;
    font-size: 0.95rem;
    color: $ink;
}

@include tablet {
    .docs-content {
        padding-top: 2rem;
    }

    .page-nav {
        grid-template-columns: 1fr;

        .page-nav-link.prev,
        .page-nav-link.next {
            grid-column: 1;
            text-align: left;
        }
    }
}
</style>
