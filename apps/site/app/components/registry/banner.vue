<script setup lang="ts">
import { cover } from "~/lib/cover"

/**
 * The picture across the top of a mod's or an owner's page: generated from their name, so
 * every page has one and it's always the same for them. The slot sits in its top-right corner.
 */
const props = defineProps<{ seed: string }>()
const art = computed(() => cover(props.seed, 120))
</script>

<template>
    <div class="banner">
        <home-art class="art" :art="art" />
        <div class="corner"><slot /></div>
    </div>
</template>

<style scoped lang="scss">
.banner {
    position: relative;
    height: 9rem;
    overflow: hidden; // the picture's corners follow the frame's
    border: 1px solid $glass-edge;
    border-radius: $radius;

    .art {
        position: absolute;
        inset: 0;
    }
}

.corner {
    position: absolute;
    top: 0.75rem;
    right: 0.75rem;
}
</style>
