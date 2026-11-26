<script setup lang="ts">
import type { Listing } from "~/registry"
import { cover } from "~/lib/cover"
import { format } from "~/lib/format"

/** The registry's banner: one listing at a time, wide, with dots to step through the rest. */
const props = defineProps<{ listings: readonly Listing[] }>()
const at = ref(0)
const shown = computed(() => props.listings[Math.min(at.value, props.listings.length - 1)])
const art = computed(() => cover(shown.value?.name ?? "", 96))
const install = useCopy()
const publish = useCopy()
const empty = cover("@terraverse/registry", 96)
const command = computed(() => `terra install ${shown.value?.name ?? ""}`)
</script>

<template>
    <section v-if="shown" class="featured">
        <home-art class="art" :art="art" />
        <div class="shade" />
        <div class="text">
            <span class="eyebrow">Featured{{ shown.official ? " · Official" : "" }}</span>
            <div class="name">
                <registry-monogram :title="shown.title" size="lg" />
                <div>
                    <h3>{{ shown.title }}</h3>
                    <span class="by">by {{ shown.owner }} · ★ {{ format.count(shown.stars) }} · ↓ {{ format.count(shown.installs) }} · v{{ shown.latestVersion }}</span>
                </div>
            </div>
            <p>{{ shown.description }}</p>
            <div class="actions">
                <home-button :to="`/registry/${shown.name}`" variant="primary">View »</home-button>
                <button type="button" class="install" @click="install.copy(command)">
                    <span class="prompt">$</span> {{ command }}
                    <span class="state">{{ install.label.value }}</span>
                </button>
            </div>
        </div>
        <nav v-if="listings.length > 1" class="dots" aria-label="Featured">
            <button
                v-for="(l, i) in listings"
                :key="l.name"
                type="button"
                :class="{ on: i === at }"
                :aria-label="l.title"
                @click="at = i"
            />
        </nav>
    </section>
    <section v-else class="featured">
        <home-art class="art" :art="empty" />
        <div class="shade" />
        <div class="text">
            <span class="eyebrow">Featured</span>
            <h3>Nothing here yet</h3>
            <p>The first mods, games and worlds land here. Make something and it could be one of them.</p>
            <div class="actions">
                <home-button to="/docs/mods/publish" variant="primary">Publish »</home-button>
                <button type="button" class="install" @click="publish.copy('terra publish')">
                    <span class="prompt">$</span> terra publish
                    <span class="state">{{ publish.label.value }}</span>
                </button>
            </div>
        </div>
    </section>
</template>

<style scoped lang="scss">
.featured {
    position: relative;
    display: grid;
    min-height: 300px;
    overflow: hidden;
    border: 1px solid $glass-edge;
    border-radius: $radius;
}

.art {
    position: absolute;
    inset: 0;
}

.shade {
    position: absolute;
    inset: 0;
    background: linear-gradient(90deg, rgba($night-0, 0.95) 0%, rgba($night-0, 0.75) 45%, rgba($night-0, 0) 80%);
}

.text {
    position: relative;
    display: grid;
    align-content: end;
    gap: 1rem;
    max-width: 36rem;
    padding: 2rem;
}

.eyebrow {
    font-family: $font-display;
    font-size: 0.65rem;
    text-transform: uppercase;
    color: $primary-ink;
}

.name {
    display: flex;
    gap: 1rem;
    align-items: center;
    min-width: 0;

    > div {
        display: grid;
        gap: 0.35rem;
        min-width: 0;
    }
}


h3 {
    margin: 0;
    font-family: $font-display;
    font-weight: 400;
    font-size: 1.5rem;
    text-transform: uppercase;
    color: $ink;
}

.by {
    font-size: 0.72rem;
    color: $ink-dim;
}

p {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.95rem;
    line-height: 1.55;
    color: $ink-dim;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    align-items: center;
}

.install {
    height: 2.6rem;
    padding: 0 1rem;
    font-family: $font-mono;
    font-size: 0.75rem;
    color: $ink-dim;
    background: rgba($night-sky, 0.8);
    border: 1px solid $glass-edge;
    border-radius: $radius;
    cursor: pointer;

    .prompt {
        color: $primary-ink;
    }

    .state {
        margin-left: 0.8rem;
        color: $ink-faint;
    }

    &:hover {
        color: $ink;
        border-color: $primary-ink;

        .state {
            color: $primary-ink;
        }
    }
}

.dots {
    position: absolute;
    right: 1.2rem;
    bottom: 1.1rem;
    display: flex;
    gap: 0.4rem;

    button {
        width: 0.7rem;
        height: 0.7rem;
        padding: 0;
        background: $night-4;
        border: none;
        cursor: pointer;

        &.on {
            background: $primary-ink;
        }
    }
}

@include mobile {
    .text {
        padding: 1.25rem;
    }

    .shade {
        background: rgba($night-0, 0.82);
    }
}
</style>
