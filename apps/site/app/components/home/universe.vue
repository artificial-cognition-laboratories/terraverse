<script setup lang="ts">
import { links, universe } from "~/config/home"
import { SYSTEM_ICONS } from "~/config/icons"

// One region, layer by layer: the list on the left chooses what the map shows.
// It opens on the terrain, the layer that reads at a glance.
const at = ref(1)
const layer = computed(() => universe.layers[at.value] ?? universe.layers[0])
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
                    :alt="`${l.title} of ${universe.code}`"
                    draggable="false"
                />
                <figcaption v-if="layer"><span class="mark">»</span> {{ universe.code }} · {{ layer.title }}</figcaption>
            </figure>
        </div>

        <div class="systems">
            <NuxtLink v-for="f in universe.systems" :key="f.title" :to="f.to" class="system">
                <span class="icon"><home-pixel-icon :icon="SYSTEM_ICONS[f.icon]" :size="36" /></span>
                <h3>{{ f.title }}</h3>
                <p>{{ f.text }}</p>
            </NuxtLink>
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
        background: transparent;
        border: 1px solid transparent;
        border-left: 3px solid $night-3;
        cursor: pointer;
        transition: background 0.1s, border-color 0.1s;

        &:hover {
            background: $night-0;
        }

        &.on {
            background: $night-0;
            border-color: $night-3;
            border-left-color: $primary-ink;

            .n {
                color: $primary-ink;
                border-color: $primary-lo;
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
    border: 1px solid $night-3;
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
    border: 1px solid $night-3;

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

    figcaption {
        position: absolute;
        left: 1rem;
        bottom: 1rem;
        padding: 0.4rem 0.7rem;
        font-family: $font-mono;
        font-size: 0.7rem;
        color: $ink;
        background: rgba($night-0, 0.8);
        border: 1px solid rgba($ink, 0.08);
    }

    .mark {
        color: $primary-ink;
    }
}

.systems {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 1rem;
    margin-top: 3rem;
}

.system {
    @include panel($night-0);
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
    min-width: 0;
    padding: 1.5rem;
    text-decoration: none;
    transition: border-color 0.1s;

    &:hover {
        border-color: $night-4;

        h3 {
            color: $primary-ink;
        }
    }
}

.icon {
    display: grid;
    place-items: center;
    width: 4rem;
    height: 4rem;
    margin-bottom: 0.6rem;
    background: $night-1;
    border: 1px solid $night-3;
}

h3 {
    margin: 0;
    font-family: $font-sans;
    font-size: 1rem;
    color: $ink;
}

p {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.88rem;
    line-height: 1.55;
    color: $ink-dim;
}

@include desktop-down {
    .viewer {
        grid-template-columns: minmax(0, 1fr);
    }

    .layers {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }

    .systems {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@include mobile {
    .layers,
    .systems {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
