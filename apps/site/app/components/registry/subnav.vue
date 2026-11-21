<script setup lang="ts">
import { ICONS } from "~/config/icons"

/** Under the topnav on every registry page: where you can go, and a search that takes you to Browse. */
const route = useRoute()
const query = ref(typeof route.query.q === "string" ? route.query.q : "")
watch(() => route.query.q, q => (query.value = typeof q === "string" ? q : ""))

const LINKS = [
    { label: "Discover", to: "/registry", exact: true },
    { label: "Browse", to: "/registry/browse", exact: false },
]

function active(link: (typeof LINKS)[number]): boolean {
    return link.exact ? route.path === link.to : route.path.startsWith(link.to)
}

async function submit() {
    await navigateTo({ path: "/registry/browse", query: query.value ? { q: query.value } : {} })
}
</script>

<template>
    <div class="subnav">
        <div class="inner">
            <nav class="links">
                <NuxtLink v-for="l in LINKS" :key="l.to" :to="l.to" class="link" :class="{ on: active(l) }">
                    {{ l.label }}
                </NuxtLink>
            </nav>
            <form class="search" role="search" @submit.prevent="submit">
                <home-pixel-icon :icon="ICONS.search" :size="14" class="glass" />
                <input v-model="query" type="search" placeholder="Search mods, worlds, shaders, authors…" aria-label="Search the registry" />
            </form>
        </div>
    </div>
</template>

<style scoped lang="scss">
.subnav {
    border-bottom: 1px solid $glass-edge;
    background: rgba($night-sky, 0.6);
    backdrop-filter: blur(8px);
}

.inner {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 2rem;
    max-width: $site-width;
    height: 3.25rem;
    margin: 0 auto;
    padding: 0 2rem;
}

.links {
    display: flex;
    align-self: stretch;
    gap: 1.75rem;
}

.link {
    display: flex;
    align-items: center;
    font-family: $font-display;
    font-size: 0.68rem;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    color: $ink-faint;
    text-decoration: none;
    border-bottom: 1px solid transparent;
    margin-bottom: -1px;

    &:hover {
        color: $ink;
    }

    &.on {
        color: $primary-ink;
        border-bottom-color: $primary-ink;
    }
}

.search {
    @include panel;
    display: flex;
    align-items: center;
    gap: 0.6rem;
    width: min(26rem, 50%);
    height: 2.2rem;
    padding: 0 0.8rem;

    &:focus-within {
        border-color: rgba($primary-ink, 0.55);
    }

    .glass {
        flex: none;
        color: $ink-faint;
    }

    input {
        flex: 1;
        min-width: 0;
        font-family: $font-mono;
        font-size: 0.75rem;
        color: $ink;
        background: none;
        border: none;
        outline: none;

        &::placeholder {
            color: $ink-faint;
        }
    }
}

@include mobile {
    .inner {
        padding: 0 1.25rem;
        gap: 1rem;
    }

    .search {
        width: auto;
        flex: 1;
    }
}
</style>
