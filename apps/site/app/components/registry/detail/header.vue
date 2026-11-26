<script setup lang="ts">
import type { Mod } from "~/registry"

/**
 * The top of a mod's page: its banner, name, owner and words. The address sits
 * quietly on the banner; clicking it copies the line that installs this version.
 */
const props = defineProps<{ mod: Mod; version: string }>()
const install = useCopy()
const command = computed(() => `terra install ${props.mod.name}${props.version === props.mod.latestVersion ? "" : `@${props.version}`}`)
const owner = computed(() => `@${props.mod.owner}`)
const address = computed(() => `${props.mod.name}${props.version === props.mod.latestVersion ? "" : `@${props.version}`}`)
</script>

<template>
    <header class="header">
        <registry-banner :seed="mod.name">
            <button type="button" class="address" :title="command" @click="install.copy(command)">
                <code>{{ address }}</code>
                <span class="state">{{ install.label.value }}</span>
            </button>
        </registry-banner>
        <div class="title">
            <registry-monogram :title="mod.title" size="lg" />
            <div class="names">
                <div class="line">
                    <h1>{{ mod.title }}</h1>
                    <span class="badge">v{{ version }}</span>
                    <span v-if="mod.official" class="badge official">Official</span>
                </div>
                <div class="byline">
                    <span class="by">
                        by <NuxtLink :to="`/registry/${owner}`">{{ mod.owner }}</NuxtLink>
                    </span>
                    <registry-star :name="mod.name" :stars="mod.stars" />
                </div>
            </div>
        </div>
        <p class="description">{{ mod.description }}</p>
        <p v-if="mod.deprecated" class="deprecated">Deprecated: {{ mod.deprecated }}</p>
    </header>
</template>

<style scoped lang="scss">
.header {
    @include on-sky;
    display: grid;
    gap: 1.25rem;
}

// The address, copied as an install line: quiet until it's wanted
.address {
    display: flex;
    gap: 0.6rem;
    align-items: center;
    padding: 0.35rem 0.6rem;
    font-family: $font-mono;
    font-size: 0.72rem;
    color: $ink-dim;
    background: rgba($night-sky, 0.55);
    backdrop-filter: blur(6px);
    border: 1px solid $glass-edge;
    border-radius: $radius;
    cursor: pointer;

    // Takes no room until it's wanted, so the chip is only the address at rest
    .state {
        display: none;
        color: $ink-faint;
    }

    &:hover,
    &:focus-visible {
        color: $ink;
        border-color: rgba($primary-ink, 0.55);

        .state {
            display: inline;
        }
    }
}

.title {
    display: flex;
    gap: 1.1rem;
    align-items: center;
    margin-top: -3.25rem;
    padding: 0 1.25rem;
    position: relative;
}

.names {
    flex: 1;
    display: grid;
    gap: 0.35rem;
    min-width: 0;
    padding-top: 2.5rem;
}

.line {
    min-width: 0;
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    align-items: center;
}

// The owner, and the star at the far end: its own row, so a narrow title never wraps it between the name and the owner
.byline {
    display: flex;
    gap: 1rem;
    align-items: center;
    justify-content: space-between;
}

// a long name (one word, as a scope is) wraps rather than pushing the page wider than a phone
h1 {
    min-width: 0;
    overflow-wrap: anywhere;
    margin: 0;
    font-family: $font-display;
    font-weight: 400;
    font-size: clamp(1.4rem, 3vw, 2rem);
    text-transform: uppercase;
    color: $ink;
}

.badge {
    padding: 0.2rem 0.45rem;
    font-family: $font-mono;
    font-size: 0.68rem;
    color: $ink-dim;
    border: 1px solid $glass-edge;
    border-radius: $radius;

    &.official {
        font-family: $font-display;
        font-size: 0.55rem;
        text-transform: uppercase;
        color: $primary-ink;
        border-color: rgba($primary-ink, 0.55);
    }
}

.by {
    font-size: 0.78rem;
    color: $ink-faint;

    a {
        color: $primary-ink;
        text-decoration: none;

        &:hover {
            color: $ink;
        }
    }
}

.description {
    margin: 0;
    max-width: 44rem;
    font-family: $font-sans;
    font-size: 1.05rem;
    line-height: 1.6;
    color: $ink-dim;
}

.deprecated {
    margin: 0;
    padding: 0.75rem 1rem;
    font-size: 0.85rem;
    color: $status-warn;
    border: 1px solid rgba($status-warn, 0.5);
    border-radius: $radius;
}
</style>
