<script setup lang="ts">
import { build, links } from "~/config/home"
import { highlight } from "~/lib/highlight"

const { data: html } = await useAsyncData("build-code", () => highlight(build.tabs))
const at = ref(0)
const shown = computed(() => html.value?.[at.value])
</script>

<template>
    <home-band>
        <div class="split">
            <div>
                <home-heading :kicker="build.kicker" :title="build.title" :lede="build.lede" />
                <home-points :points="build.points" />
                <div class="actions">
                    <home-button :to="links.mods" variant="primary">Write a mod »</home-button>
                    <home-button :to="links.cli">The CLI</home-button>
                </div>
            </div>
            <home-window class="panel">
                <template #bar>
                    <div class="tabs" role="tablist">
                        <button
                            v-for="(t, i) in build.tabs"
                            :key="t.label"
                            type="button"
                            role="tab"
                            :aria-selected="i === at"
                            :class="{ on: i === at }"
                            @click="at = i"
                        >
                            {{ t.label }}
                        </button>
                    </div>
                </template>
                <home-code v-if="shown" :html="shown" />
            </home-window>
        </div>
    </home-band>
</template>

<style scoped lang="scss">
.split {
    display: grid;
    grid-template-columns: minmax(0, 5fr) minmax(0, 6fr);
    gap: 4rem;
    align-items: center;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    margin-top: 2.25rem;
}

// Every tab the same height, so switching never moves the page
.panel {
    min-height: 27rem;
}

// Tabs sit at the window's top-right, the chosen one underlined
.tabs {
    display: flex;
    align-self: stretch;

    button {
        padding: 0 0.9rem;
        font-family: $font-display;
        font-size: 0.62rem;
        text-transform: uppercase;
        color: $ink-faint;
        background: none;
        border: none;
        border-bottom: 1px solid transparent;
        cursor: pointer;

        &:hover {
            color: $ink;
        }

        &.on {
            color: $primary-ink;
            border-bottom-color: $primary-ink;
        }
    }
}

@include tablet {
    .split {
        grid-template-columns: minmax(0, 1fr);
        gap: 3rem;
    }

    .panel {
        min-height: 0;
    }
}
</style>
