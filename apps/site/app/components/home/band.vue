<script setup lang="ts">
/** One of the page's horizontal bands: full width, its content held to the site's measure. */
withDefaults(defineProps<{ tone?: "base" | "lifted" }>(), { tone: "base" })
</script>

<template>
    <section class="band" :class="tone">
        <div class="inner">
            <slot />
        </div>
    </section>
</template>

<style scoped lang="scss">
.band {
    padding: 6.5rem 0;

    &.base {
        background: $stone-0;
    }

    &.lifted {
        background: $stone-1;
        border-top: 1px solid $stone-2;
        border-bottom: 1px solid $stone-2;
    }
}

.inner {
    max-width: $site-width;
    margin: 0 auto;
    padding: 0 2rem;
}

@include mobile {
    .band {
        padding: 4.5rem 0;
    }

    .inner {
        padding: 0 1.25rem;
    }
}
</style>
