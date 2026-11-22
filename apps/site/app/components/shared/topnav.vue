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
                <shared-profile />
            </nav>

            <span class="profile-narrow"><shared-profile /></span>
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
    background: rgba($night-0, 0.72);
    backdrop-filter: blur(10px);
    border-bottom: 1px solid $glass-edge;
}

.topnav-inner {
    max-width: $site-width;
    height: $topnav-height;
    margin: 0 auto;
    padding: 0 2rem;
    display: flex;
    align-items: center;
    justify-content: space-between;
}

.mark {
    font-family: $font-display;
    font-size: 1.35rem;
    color: $primary-ink;
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
    color: $ink-dim;
    text-decoration: none;

    &:hover {
        color: $ink;
    }

    &.active {
        color: $primary-ink;
    }
}

.icon-link {
    display: flex;
    color: $primary-ink;
    font-size: 1.1rem;
}

.burger {
    display: none;
    background: transparent;
    border: 1px solid $glass-edge;
    color: $ink;
    width: 2rem;
    height: 2rem;
    font-size: 1.1rem;
    cursor: pointer;
}

.drawer {
    display: none;
}

// On a phone the links fold into the drawer; the account stays beside the burger
.profile-narrow {
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

    .profile-narrow {
        display: flex;
        margin-left: auto;
        margin-right: 1rem;
    }

    .drawer.open {
        display: block;
        position: fixed;
        top: $topnav-height;
        left: 0;
        right: 0;
        bottom: 0;
        overflow-y: auto;
        background: $night-sky;
        padding: 1rem 1.25rem 2rem;
    }

    .drawer-links {
        display: flex;
        gap: 1.25rem;
        margin-bottom: 1.5rem;
        padding-bottom: 1rem;
        border-bottom: 1px solid $glass-edge;
    }
}
</style>
