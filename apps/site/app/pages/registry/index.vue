<script setup lang="ts">
import { flags } from "~/config/home"
import { useRegistry } from "~/registry"

definePageMeta({ layout: "landing" })
useHead({ title: "Registry" })

const reg = useRegistry()
const { data: front } = await useLive("registry:front", () => reg.front())
const { data: servers } = await useLive("registry:servers", () => reg.servers(), { every: 30_000 })
const publish = useCopy()
</script>

<template>
    <registry-shell>
        <template v-if="front">
            <home-heading
                kicker="The registry"
                title="Made by everyone."
                :lede="front.total ? `${front.total} mods, games, worlds and packs, made by the community and by us. Add one to a world in a click, or publish your own with one command.` : 'Mods, games, worlds and packs, made by the community and by us. It\'s just opening: publish the first with one command.'"
                :link="{ label: 'Browse all', to: '/registry/browse' }"
            />

            <registry-featured :listings="front.featured" />

            <registry-row-head label="Categories" />
            <registry-categories :categories="front.categories" />

            <template v-for="shelf in front.shelves" :key="shelf.id">
                <registry-row-head
                    :label="shelf.title"
                    :link="{ label: 'See all', to: { path: '/registry/browse', query: shelf.search.official ? { official: '1', sort: shelf.search.sort } : { sort: shelf.search.sort } } }"
                />
                <registry-cards :listings="shelf.listings" :fill="4" />
            </template>

            <template v-if="flags.servers && servers">
                <registry-row-head label="Live servers" />
                <registry-servers :servers="servers" />
            </template>

            <section class="publish">
                <div>
                    <h2>Made something?</h2>
                    <p>Publish it from the folder it's in. It's typed, versioned and one install away for everyone.</p>
                </div>
                <button type="button" class="command" @click="publish.copy('terra publish')">
                    <span class="prompt">$</span> terra publish
                    <span class="state">{{ publish.label.value }}</span>
                </button>
                <home-button to="/docs/mods/publish">How »</home-button>
            </section>
        </template>
    </registry-shell>
</template>

<style scoped lang="scss">
.publish {
    @include panel;
    display: flex;
    align-items: center;
    gap: 1.5rem;
    margin-top: 4rem;
    padding: 1.75rem 2rem;

    > div {
        flex: 1;
        min-width: 0;
    }

    h2 {
        margin: 0 0 0.4rem;
        font-family: $font-sans;
        font-size: 1.2rem;
        color: $ink;
    }

    p {
        margin: 0;
        font-family: $font-sans;
        font-size: 0.92rem;
        color: $ink-dim;
    }
}

.command {
    @include well;
    padding: 0.7rem 1rem;
    font-family: $font-mono;
    font-size: 0.85rem;
    color: $ink;
    cursor: pointer;

    .prompt {
        color: $primary-ink;
    }

    .state {
        margin-left: 0.8rem;
        color: $ink-faint;
    }
}

@include tablet {
    .publish {
        flex-direction: column;
        align-items: flex-start;
    }
}
</style>
