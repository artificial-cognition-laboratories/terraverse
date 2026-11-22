<script setup lang="ts">
/**
 * A window on the night: one block of glass with three dots at one end of its bar
 * and, at the other, a label or whatever the `bar` slot holds (tabs).
 */
withDefaults(defineProps<{ label?: string; dots?: "left" | "right" }>(), { dots: "left" })
</script>

<template>
    <figure class="window" :class="`dots-${dots}`">
        <figcaption class="bar">
            <span class="dots" aria-hidden="true"><i /><i /><i /></span>
            <slot name="bar">
                <span v-if="label" class="label">{{ label }}</span>
            </slot>
        </figcaption>
        <div class="body">
            <slot />
        </div>
    </figure>
</template>

<style scoped lang="scss">
.window {
    @include panel;
    margin: 0;
    display: flex;
    flex-direction: column;
    min-width: 0;
}

// The same glass as the body: nothing divides them
.bar {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
    min-height: 2.8rem;
    padding: 0 1rem;
    white-space: nowrap;
    overflow: hidden;
}

.dots-right .dots {
    order: 2;
}

.dots {
    display: flex;
    flex: none;
    gap: 0.4rem;

    i {
        width: 0.6rem;
        height: 0.6rem;
        border-radius: 50%;
        background: $night-4;
    }
}

.label {
    font-family: $font-mono;
    font-size: 0.72rem;
    color: $ink-faint;
}

.body {
    position: relative;
    flex: 1;
    min-height: 0;
}
</style>
