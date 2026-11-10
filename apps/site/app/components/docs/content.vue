<template>
    <div class="docs-content">
        <article class="prose">
            <slot />
        </article>

        <nav v-if="prev || next" class="page-nav">
            <NuxtLink v-if="prev" :to="prev.path" class="page-nav-link prev">
                <span class="nav-label">Previous</span>
                <span class="nav-title">{{ prev.title }}</span>
            </NuxtLink>
            <NuxtLink v-if="next" :to="next.path" class="page-nav-link next">
                <span class="nav-label">Next</span>
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
.docs-content {
    max-width: 750px;
    padding: calc(2rem + 15px) 0.25rem 4rem;
    width: 100%;
    min-width: 0;
}

.prose {
    color: $text-primary;
}

.page-nav {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1.5rem;
    margin-top: 5rem;
    padding-top: 2.5rem;
    border-top: 1px solid rgba($primary, 0.15);
}

.page-nav-link {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    padding: 1.25rem 1.5rem;
    border: 1px solid rgba($primary, 0.15);
    text-decoration: none;

    &:hover {
        background: rgba($primary, 0.05);
        border-color: $primary;
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
    font-size: 0.6875rem;
    color: $text-tertiary;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-weight: 600;
}

.nav-title {
    font-size: 0.9375rem;
    color: $text-primary;
    font-weight: 600;
}

@include tablet {
    .docs-content {
        padding: 2rem 0;
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
