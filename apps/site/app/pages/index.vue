<script setup lang="ts">
import { flags, hero } from "~/config/home"

definePageMeta({ layout: "landing" })
useHead({ title: "" })
</script>

<template>
    <div class="home">
        <home-sky :seed="hero.code" />
        <home-hero />
        <home-limits />
        <home-universe v-if="flags.universe" />
        <home-registry v-if="flags.registry" />
        <home-hosting v-if="flags.hosting" />
        <home-build />
        <home-agents v-if="flags.agents" />
        <home-open />
        <home-ground :seed="hero.code">
            <shared-footer />
        </home-ground>
    </div>
</template>

<style scoped lang="scss">
// The sky sits behind every section, inside this page and nowhere else
.home {
    position: relative;
    isolation: isolate;
}
</style>
