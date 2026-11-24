<script setup lang="ts">
import { flags, links, registry } from "~/config/home"
import { useRegistry } from "~/registry"

const reg = useRegistry()
const { data: front } = await useLive("registry:front", () => reg.front())
const { data: servers } = await useLive("registry:servers", () => reg.servers(), { every: 30_000 })
const trending = computed(() => front.value?.shelves.find(s => s.id === "trending")?.listings ?? [])
</script>

<template>
    <home-band v-if="front">
        <home-heading
            :kicker="registry.kicker"
            :title="registry.title"
            :lede="registry.lede"
            :link="{ label: 'Browse all', to: links.browse }"
        />

        <registry-featured :listings="front.featured" />

        <registry-row-head label="Categories" />
        <registry-categories :categories="front.categories" />

        <registry-row-head label="Trending" :link="{ label: 'See all', to: { path: links.browse, query: { sort: 'trending' } } }" />
        <registry-cards :listings="trending" :fill="4" />

        <template v-if="flags.servers && servers">
            <registry-row-head label="Live servers" :link="{ label: 'All servers', to: links.registry }" />
            <registry-servers :servers="servers" />
        </template>
    </home-band>
</template>
