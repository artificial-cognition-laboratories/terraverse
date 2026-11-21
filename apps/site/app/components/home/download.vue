<script setup lang="ts">
import { platforms } from "~/config/downloads"

/** Every way to get the game: the tiles, and the visitor's one-line install under them. */
const { detected } = usePlatform()
const command = computed(() => (platforms.find(p => p.id === detected.value) ?? platforms[0])?.command)
const copy = useCopy()
</script>

<template>
    <div class="download">
        <home-download-tiles />
        <button v-if="command" type="button" class="command" @click="copy.copy(command)">
            <span class="prompt">$</span>
            <code>{{ command }}</code>
            <span class="state">{{ copy.label.value }}</span>
        </button>
    </div>
</template>

<style scoped lang="scss">
.command {
    @include panel($stone-0);
    display: flex;
    gap: 0.8rem;
    align-items: center;
    width: 100%;
    margin-top: 0.75rem;
    padding: 0.9rem 1.1rem;
    font-family: $font-mono;
    font-size: 0.82rem;
    text-align: left;
    color: $ink;
    cursor: pointer;

    code {
        flex: 1;
        min-width: 0;
        overflow-x: auto;
        white-space: nowrap;
        scrollbar-width: none;
    }

    .prompt {
        color: $primary-ink;
    }

    .state {
        color: $ink-faint;
    }

    &:hover {
        border-color: $stone-4;

        .state {
            color: $primary-ink;
        }
    }
}
</style>
