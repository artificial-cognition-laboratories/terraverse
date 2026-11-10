<template>
    <header class="topnav">
        <div class="topnav-inner">
            <NuxtLink to="/" class="mark"><span>»</span> terraverse</NuxtLink>

            <nav class="links">
                <NuxtLink
                    v-for="link in navLinks"
                    :key="link.to"
                    :to="link.to"
                    class="link"
                    :class="{ active: route.path.startsWith(link.to) }"
                >
                    {{ link.label }}
                </NuxtLink>
                <a :href="GITHUB" class="icon-link" aria-label="GitHub">
                    <Icon name="simple-icons:github" />
                </a>
            </nav>

            <button class="burger" :aria-expanded="open" aria-label="Menu" @click="open = !open">
                {{ open ? "×" : "≡" }}
            </button>
        </div>

        <!-- Always in the DOM: pages teleport their sidebar into
             #mobile-page-nav, and a Teleport needs its target to exist. -->
        <div class="drawer" :class="{ open }">
            <nav class="drawer-links">
                <NuxtLink v-for="link in navLinks" :key="link.to" :to="link.to" class="link">
                    {{ link.label }}
                </NuxtLink>
                <a :href="GITHUB" class="link">github</a>
            </nav>
            <div id="mobile-page-nav" />
        </div>
    </header>
</template>

<script setup lang="ts">
import { GITHUB, navLinks } from "~/config/site"

const route = useRoute()
const open = ref(false)

// Any navigation closes the drawer
watch(() => route.fullPath, () => (open.value = false))
</script>

<style scoped lang="scss">
.topnav {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    z-index: 1000;
    background: $bg-primary;
}

.topnav-inner {
    max-width: $site-width;
    height: 52px;
    margin: 0 auto;
    padding: 0 2rem;
    display: flex;
    align-items: center;
    justify-content: space-between;
}

.mark {
    font-family: $font-display;
    font-size: 1.35rem;
    color: $primary;
    text-decoration: none;
    white-space: nowrap;

    span {
        margin-right: 0.15em;
    }
}

.links {
    display: flex;
    align-items: center;
    gap: 1.5rem;
}

.link {
    font-size: 0.85rem;
    color: $text-secondary;
    text-decoration: none;

    &:hover {
        color: $text-primary;
    }

    &.active {
        color: $primary;
    }
}

.icon-link {
    display: flex;
    color: $primary;
    font-size: 1.1rem;
}

.burger {
    display: none;
    background: transparent;
    border: 1px solid $border-mid;
    color: $text-primary;
    width: 2rem;
    height: 2rem;
    font-size: 1.1rem;
    cursor: pointer;
}

.drawer {
    display: none;
}

@include tablet {
    .topnav-inner {
        padding: 0 1.25rem;
    }

    .links {
        display: none;
    }

    .burger {
        display: block;
    }

    .drawer.open {
        display: block;
        position: fixed;
        top: 52px;
        left: 0;
        right: 0;
        bottom: 0;
        overflow-y: auto;
        background: $bg-primary;
        padding: 1rem 1.25rem 2rem;
        border-top: 1px solid $border-soft;
    }

    .drawer-links {
        display: flex;
        gap: 1.25rem;
        margin-bottom: 1.5rem;
        padding-bottom: 1rem;
        border-bottom: 1px solid $border-soft;
    }
}
</style>
