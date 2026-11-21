<script setup lang="ts">
import { links } from "~/config/home"

definePageMeta({ layout: "landing" })
useHead({ title: "Registry" })
</script>

<template>
    <home-band>
        <div class="notice">
            <home-heading
                kicker="The registry"
                title="Not on the web yet."
                lede="The registry's web view is being built. Until it's here, browse mods, worlds and servers from Community in the game, or publish your own from the CLI."
            />
            <div class="actions">
                <home-button to="/#download" variant="primary">Download the game</home-button>
                <home-button :to="links.mods">Publish a mod »</home-button>
            </div>
        </div>
    </home-band>
</template>

<style scoped lang="scss">
.notice {
    padding: 6rem 0 4rem;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
}
</style>
