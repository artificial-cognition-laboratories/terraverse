<script setup lang="ts">
import { limits } from "~/config/home"
import { ICONS } from "~/config/icons"

// `like this` in a line is a command: set it as code
function parts(text: string) {
    return text.split("`").map((part, i) => ({ text: part, code: i % 2 === 1 }))
}
</script>

<template>
    <home-band>
        <home-heading :kicker="limits.kicker" :title="limits.title" :lede="limits.lede" />
        <ul class="grid">
            <li v-for="item in limits.items" :key="item.now" class="card">
                <home-pixel-icon class="icon" :icon="ICONS[item.icon]" :size="36" />
                <h3>{{ item.now }}</h3>
                <p>
                    <template v-for="(part, i) in parts(item.text)" :key="i">
                        <code v-if="part.code">{{ part.text }}</code>
                        <template v-else>{{ part.text }}</template>
                    </template>
                </p>
            </li>
        </ul>
    </home-band>
</template>

<style scoped lang="scss">
.grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 1rem;
    margin: 0;
    padding: 0;
    list-style: none;
}

.card {
    @include panel;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    min-width: 0;
    padding: 1.75rem;
}

.icon {
    margin-bottom: 1rem;
}

h3 {
    margin: 0;
    font-family: $font-sans;
    font-size: 1.1rem;
    line-height: 1.3;
    color: $ink;
}

p {
    margin: 0.25rem 0 0;
    font-family: $font-sans;
    font-size: 0.9rem;
    line-height: 1.6;
    color: $ink-dim;
}

code {
    font-family: $font-mono;
    font-size: 0.85em;
    color: $primary-ink;
}

@include desktop-down {
    .grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@include mobile {
    .grid {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
