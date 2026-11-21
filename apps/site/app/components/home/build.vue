<script setup lang="ts">
import { build, links } from "~/config/home"
import { highlight } from "~/lib/highlight"

const { data: html } = await useAsyncData("build-code", () => highlight(build.tabs))
const at = ref(0)
const tab = computed(() => build.tabs[at.value] ?? build.tabs[0])
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
            <div class="panel">
                <div class="bar">
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
                    <span v-if="tab" class="file">{{ tab.file }}</span>
                </div>
                <home-code v-if="shown" :html="shown" />
            </div>
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
    min-width: 0;
    min-height: 26rem;
}

.bar {
    @include on-sky;
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 1rem;
    border-bottom: 1px solid $night-3;
}

.tabs {
    display: flex;

    button {
        padding: 0.7rem 1.1rem;
        font-family: $font-display;
        font-size: 0.65rem;
        text-transform: uppercase;
        color: $ink-faint;
        background: none;
        border: none;
        border-bottom: 2px solid transparent;
        margin-bottom: -1px;
        cursor: pointer;

        &:first-child {
            padding-left: 0;
        }

        &:hover {
            color: $ink;
        }

        &.on {
            color: $primary-ink;
            border-bottom-color: $primary-ink;
        }
    }
}

.file {
    padding-bottom: 0.7rem;
    font-family: $font-mono;
    font-size: 0.7rem;
    color: $ink-faint;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
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
