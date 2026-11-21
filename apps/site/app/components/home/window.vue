<script setup lang="ts">
/** A framed panel with the game's title bar: `» title` on the left, an optional note on the right. */
defineProps<{ title: string; note?: string }>()
</script>

<template>
    <figure class="window">
        <figcaption class="bar">
            <span><span class="mark" aria-hidden="true">»</span> {{ title }}</span>
            <span v-if="note" class="note">{{ note }}</span>
        </figcaption>
        <div class="body">
            <slot />
        </div>
    </figure>
</template>

<style scoped lang="scss">
.window {
    margin: 0;
    display: flex;
    flex-direction: column;
    min-width: 0;
    @include panel;
    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.25);
}

.bar {
    display: flex;
    justify-content: space-between;
    gap: 1rem;
    padding: 0.7rem 1rem;
    font-family: $font-display;
    font-size: 0.68rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $ink;
    background: $night-2;
    border-bottom: 1px solid $night-3;
    white-space: nowrap;
    overflow: hidden;
}

.mark {
    color: $primary-ink;
}

.note {
    color: $ink-faint;
    overflow: hidden;
    text-overflow: ellipsis;
}

.body {
    position: relative;
    flex: 1;
    min-height: 0;
}
</style>
