<template>
    <footer class="footer">
        <div class="footer-inner">
            <span class="mark">» terraverse</span>
            <span class="credit">
                Open source, built on <a href="https://github.com/luanti-org/luanti">Luanti</a>.
                An <a href="https://arclabs.it">ArcLabs</a> project.
            </span>
            <nav class="links">
                <NuxtLink v-for="link in navLinks" :key="link.to" :to="link.to">{{ link.label }}</NuxtLink>
                <a :href="GITHUB">github</a>
            </nav>
        </div>
    </footer>
</template>

<script setup lang="ts">
import { GITHUB, navLinks } from "~/config/site"
</script>

<style scoped lang="scss">
.footer {
    border-top: 1px solid $border-soft;
}

.footer-inner {
    max-width: $site-width;
    margin: 0 auto;
    padding: 2rem;
    display: flex;
    flex-wrap: wrap;
    gap: 1rem 2rem;
    align-items: baseline;
    font-size: 0.75rem;
    color: $text-tertiary;
}

.mark {
    font-family: $font-display;
    color: $primary;
}

.credit {
    flex: 1;
}

a {
    color: $text-secondary;
    text-decoration: none;

    &:hover {
        color: $primary;
    }
}

.links {
    display: flex;
    gap: 1.25rem;
}

@include mobile {
    .footer-inner {
        padding: 2rem 1rem;
    }
}
</style>
