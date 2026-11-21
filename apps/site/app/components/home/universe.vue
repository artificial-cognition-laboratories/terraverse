<script setup lang="ts">
import { links, universe } from "~/config/home"

// One region, layer by layer: the list on the left chooses what the map shows.
// It opens on the terrain, the layer that reads at a glance.
const at = ref(1)
</script>

<template>
    <home-band tone="lifted">
        <home-heading
            :kicker="universe.kicker"
            :title="universe.title"
            :lede="universe.lede"
            :link="{ label: 'See it in the wiki', to: links.wiki }"
        />

        <div class="viewer">
            <ol class="layers" role="tablist">
                <li v-for="(l, i) in universe.layers" :key="l.id">
                    <button type="button" role="tab" :aria-selected="i === at" :class="{ on: i === at }" @click="at = i">
                        <span class="n">{{ i + 1 }}</span>
                        <span class="words">
                            <strong>{{ l.title }}</strong>
                            <span>{{ l.text }}</span>
                        </span>
                    </button>
                </li>
            </ol>
            <figure class="map">
                <img
                    v-for="(l, i) in universe.layers"
                    :key="l.id"
                    :src="l.image"
                    :class="{ on: i === at }"
                    :alt="`${l.title} of one region of a generated world`"
                    draggable="false"
                />
            </figure>
        </div>

    </home-band>
</template>

<style scoped lang="scss">
// The map sets the row's height from its width; the list stays at its own height beside it
.viewer {
    display: grid;
    grid-template-columns: minmax(0, 4fr) minmax(0, 9fr);
    gap: 1.5rem;
    align-items: start;
}

.layers {
    display: grid;
    gap: 0.5rem;
    margin: 0;
    padding: 0;
    list-style: none;

    button {
        display: flex;
        gap: 1rem;
        align-items: flex-start;
        width: 100%;
        height: 100%;
        padding: 1rem 1.1rem;
        text-align: left;
        color: inherit;
        @include panel;
        cursor: pointer;
        transition: border-color 0.1s;

        &:hover {
            border-color: rgba($ink, 0.2);
        }

        // The chosen layer: its edge lit, like every chosen thing on the page
        &.on {
            border-color: rgba($primary-ink, 0.55);

            .n {
                color: $primary-ink;
                border-color: rgba($primary-ink, 0.55);
            }

            .words span {
                color: $ink-dim;
            }
        }
    }
}

.n {
    flex: none;
    display: grid;
    place-items: center;
    width: 1.8rem;
    height: 1.8rem;
    font-family: $font-display;
    font-size: 0.7rem;
    color: $ink-faint;
    border: 1px solid $glass-edge;
}

.words {
    display: grid;
    gap: 0.3rem;
    font-family: $font-sans;

    strong {
        font-size: 0.98rem;
        color: $ink;
    }

    span {
        font-size: 0.85rem;
        line-height: 1.5;
        color: $ink-faint;
    }
}

// Every layer is stacked; the chosen one fades in over the last
.map {
    position: relative;
    min-width: 0;
    width: 100%;
    margin: 0;
    aspect-ratio: 2 / 1;
    overflow: hidden;
    background: $night-0;
    border: 1px solid $glass-edge;

    img {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        object-fit: cover;
        image-rendering: pixelated;
        opacity: 0;
        transition: opacity 0.35s;

        &.on {
            opacity: 1;
        }
    }
}

@include desktop-down {
    .viewer {
        grid-template-columns: minmax(0, 1fr);
    }

    .layers {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@include mobile {
    .layers {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
