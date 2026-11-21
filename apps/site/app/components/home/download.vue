<script setup lang="ts">
import { platforms } from "~/config/downloads"

/** Every way to get the game: the tiles, and under them, as wide as they are, the one-line install. */
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
// As wide as the row of tiles, so the command lines up under them
.download {
    display: inline-flex;
    flex-direction: column;
    gap: 0.75rem;
    max-width: 100%;

    :deep(.tiles) {
        flex-wrap: nowrap;
    }
}

.command {
    @include panel;
    display: flex;
    gap: 0.8rem;
    align-items: center;
    width: 100%;
    padding: 0.95rem 1.1rem;
    font-family: $font-mono;
    font-size: 0.8rem;
    text-align: left;
    color: $ink;
    cursor: pointer;
    transition: border-color 0.1s;

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
        border-color: rgba($primary-ink, 0.55);

        .state {
            color: $primary-ink;
        }
    }
}

@include mobile {
    .download {
        display: flex;

        :deep(.tiles) {
            flex-wrap: wrap;
        }
    }
}
</style>
