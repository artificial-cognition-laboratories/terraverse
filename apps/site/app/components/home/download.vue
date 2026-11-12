<template>
    <div class="download">
        <div v-if="mine.command" class="command">
            <span class="prompt">$</span>
            <code>{{ mine.command }}</code>
            <button class="copy" :class="{ done: copied !== 'copy' }" @click="copy(mine.command)">
                {{ copied }}
            </button>
        </div>

        <div class="tiles">
            <template v-for="(p, n) in ordered" :key="p.id">
                <span v-if="n === 1" class="split" aria-hidden="true" />
                <a
                    v-for="d in p.downloads"
                    :key="d.href"
                    :href="d.href"
                    class="tile"
                    :class="{ other: n > 0 }"
                >
                    <Icon :name="d.icon" class="tile-icon" />
                    <!-- One download names its OS; several name themselves -->
                    <span class="tile-label">{{ p.downloads.length === 1 ? p.label : d.label }}</span>
                    <span class="tile-detail">{{ d.detail }}</span>
                </a>
                <div v-if="p.notReady" class="tile soon" :title="p.notReady">
                    <Icon :name="p.icon" class="tile-icon" />
                    <span class="tile-label">{{ p.label }}</span>
                    <span class="tile-detail">soon</span>
                </div>
            </template>
        </div>
    </div>
</template>

<script setup lang="ts">
import { platforms, type Platform } from "~/config/downloads"

const { detected } = usePlatform()

// The visitor's OS leads; before it's known (and on the prerendered page) the
// first platform does. Every other platform sits in the smaller row below.
const mine = computed<Platform>(() => {
    const found = platforms.find(p => p.id === detected.value) ?? platforms[0]
    if (!found) throw new Error("downloads: no platforms")
    return found
})
// Theirs first, then the rest past a gap
const ordered = computed(() => [mine.value, ...platforms.filter(p => p.id !== mine.value.id)])

const copied = ref("copy")
async function copy(text: string) {
    try {
        await navigator.clipboard.writeText(text)
        copied.value = "copied"
    } catch {
        copied.value = "select it"
    }
    setTimeout(() => (copied.value = "copy"), 1600)
}
</script>

<style scoped lang="scss">
.download {
    max-width: 34rem;
}

.tiles {
    display: flex;
    flex-wrap: wrap;
    gap: 0.6rem;

    .command + & {
        margin-top: 0.75rem;
    }
}

.tile {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.2rem;
    width: 5.5rem;
    aspect-ratio: 1;
    background: $surface-solid;
    border: 1px solid $border-mid;
    color: $text-primary;
    text-decoration: none;
    transition: $transition-standard;

    &:hover {
        border-color: $primary;
        background: color-mix(in srgb, $surface-solid 88%, $primary);

        .tile-icon {
            color: $primary;
        }
    }

    &.other {
        color: $text-secondary;
    }

    &.soon {
        color: $text-tertiary;
        opacity: 0.7;
        cursor: default;

        &:hover {
            border-color: $border-mid;
            background: $surface-solid;
        }
    }
}

.tile-icon {
    font-size: 1.3rem;
    margin-bottom: 0.25rem;
    transition: $transition-standard;
}

.tile-label {
    font-size: 0.68rem;
    font-weight: 700;
}

.tile-detail {
    font-size: 0.52rem;
    color: $text-tertiary;
    text-align: center;
    white-space: nowrap;
    padding: 0 0.5rem;
}

.command {
    display: flex;
    align-items: center;
    gap: 0.7rem;
    padding: 0 0 0 0.9rem;
    background: $surface-solid;
    border: 1px solid $border-mid;
    border-left: 2px solid $primary;

    .prompt {
        color: $primary;
        user-select: none;
    }

    code {
        flex: 1;
        min-width: 0;
        padding: 0.75rem 0;
        font-size: 0.78rem;
        color: $text-primary;
        overflow-x: auto;
        white-space: nowrap;
        scrollbar-width: none;
    }
}

// Joined to the box's right edge rather than floating inside it
.copy {
    flex: none;
    align-self: stretch;
    padding: 0 1rem;
    font-size: 0.72rem;
    color: $text-secondary;
    background: transparent;
    border: none;
    border-left: 1px solid $border-mid;
    cursor: pointer;

    &:hover {
        color: $primary;
        background: rgba($primary, 0.06);
    }

    &.done {
        color: $status-good;
    }
}

.split {
    width: 1px;
    margin: 0.75rem 0.4rem;
    background: $border-mid;
}

@include mobile {
    .tile {
        width: calc(50% - 0.3rem);
    }

    .split {
        display: none;
    }
}
</style>
