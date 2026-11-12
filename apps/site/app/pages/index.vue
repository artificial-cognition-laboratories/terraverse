<template>
    <div class="home">
        <home-hero />
        <shared-footer />
    </div>
</template>

<script setup lang="ts">
definePageMeta({ layout: "landing" })
useHead({ title: "" })
</script>

<style scoped lang="scss">
// One screen: the footer sits on the sky at the hero's foot
.home {
    position: relative;

    :deep(.footer) {
        position: absolute;
        left: 0;
        right: 0;
        bottom: 0;
        border-top: none;
        // Shade the planet under the words so they read on its bright limb
        background: linear-gradient(to bottom, transparent, rgba($bg-space, 0.85) 60%);
        text-shadow: 0 1px 6px rgba($bg-space, 0.9), 0 0 2px $bg-space;
    }
}
</style>
