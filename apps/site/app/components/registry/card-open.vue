<script setup lang="ts">
/** An empty place in a row of cards: room for the next mod, and how to make it. */
defineProps<{ first: boolean }>()
</script>

<template>
    <NuxtLink to="/docs/mods/publish" class="open">
        <span class="plus">+</span>
        <strong>{{ first ? "Be the first" : "Yours next" }}</strong>
        <span class="how"><span class="prompt">$</span> terra publish</span>
    </NuxtLink>
</template>

<style scoped lang="scss">
.open {
    display: grid;
    place-content: center;
    justify-items: center;
    gap: 0.6rem;
    min-width: 0;
    min-height: 17rem;
    text-decoration: none;
    border: 1px dashed rgba($ink, 0.14);
    border-radius: $radius;
    transition: border-color 0.1s;

    &:hover {
        border-color: rgba($primary-ink, 0.55);

        strong,
        .plus {
            color: $primary-ink;
        }
    }
}

.plus {
    font-family: $font-display;
    font-size: 1.6rem;
    line-height: 1;
    color: $ink-faint;
}

strong {
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.7rem;
    text-transform: uppercase;
    color: $ink-dim;
}

.how {
    font-family: $font-mono;
    font-size: 0.72rem;
    color: $ink-faint;

    .prompt {
        color: $primary-ink;
    }
}
</style>
