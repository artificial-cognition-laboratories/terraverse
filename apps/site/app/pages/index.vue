<script setup lang="ts">
import { flags } from "~/config/home"

definePageMeta({ layout: "landing" })
useHead({ title: "" })
</script>

<template>
    <div class="home">
        <home-hero />
        <home-facts />
        <home-survival />
        <home-universe />
        <home-registry v-if="flags.registry" />
        <home-hosting v-if="flags.hosting" />
        <home-build />
        <home-agents v-if="flags.agents" />
        <home-open />
        <shared-footer />
    </div>
</template>
