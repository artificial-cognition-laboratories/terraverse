<script setup lang="ts">
import type { Mod } from "~/registry"
import { contents, type Tab } from "~/registry/detail"

/** A phone's mod tabs: the side card's choices as a strip under the header, sticky as the page scrolls. */
const props = defineProps<{ mod: Mod; tab: Tab; version: string }>()
const emit = defineEmits<{ tab: [tab: Tab] }>()
const tabs = computed(() => contents(props.mod, props.version))
</script>

<template>
    <nav class="tabs" aria-label="This mod">
        <button v-for="t in tabs" :key="t.id" type="button" :class="{ on: t.id === tab }" @click="emit('tab', t.id)">
            {{ t.label }}<span v-if="t.count !== undefined" class="count">{{ t.count }}</span>
        </button>
    </nav>
</template>

<style scoped lang="scss">
.tabs {
    position: sticky;
    top: calc(#{$topnav-height} + #{$subnav-height});
    z-index: 10;
    display: flex;
    gap: 1.25rem;
    overflow-x: auto;
    scrollbar-width: none;
    background: rgba($night-sky, 0.72);
    backdrop-filter: blur(10px);
    border-bottom: 1px solid $glass-edge;

    button {
        display: flex;
        flex: none;
        gap: 0.4rem;
        align-items: center;
        padding: 0.8rem 0;
        font-family: $font-display;
        font-size: 0.62rem;
        text-transform: uppercase;
        color: $ink-faint;
        background: none;
        border: 0;
        border-bottom: 1px solid transparent;
        cursor: pointer;

        &.on {
            color: $primary-ink;
            border-bottom-color: $primary-ink;
        }
    }
}

.count {
    font-family: $font-mono;
    color: $ink-faint;
}
</style>
