<script setup lang="ts">
import { flags, links, registry } from "~/config/home"
import { useRegistry } from "~/registry"

const reg = useRegistry()
const { data } = await useAsyncData("home-registry", async () => ({ front: await reg.front(), servers: await reg.servers() }))
const trending = computed(() => data.value?.front.shelves.find(s => s.id === "trending")?.listings ?? [])
</script>

<template>
    <home-band v-if="data">
        <home-heading
            :kicker="registry.kicker"
            :title="registry.title"
            :lede="registry.lede"
            :link="{ label: 'Browse all', to: links.browse }"
        />

        <registry-featured :listings="data.front.featured" />

        <registry-row-head label="Categories" />
        <registry-categories :categories="data.front.categories" />

        <registry-row-head label="Trending" :link="{ label: 'See all', to: { path: links.browse, query: { sort: 'trending' } } }" />
        <registry-cards :listings="trending" />

        <template v-if="flags.servers">
            <registry-row-head label="Live servers" :link="{ label: 'All servers', to: links.registry }" />
            <registry-servers :servers="data.servers" />
        </template>
    </home-band>
</template>
