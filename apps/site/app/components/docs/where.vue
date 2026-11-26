<script setup lang="ts">
import { pagesOf } from "~/config/docs-nav"
import { ICONS } from "~/config/icons"

/**
 * On a phone, under the subnav: where you are in the docs (section › page), so their shape is
 * never hidden. Tapping it opens the drawer, where the section tabs and the tree live.
 */
const nav = useDocsNav()
const route = useRoute()
const drawer = useDrawer()
const searching = useDocsSearch()
const page = computed(() => pagesOf(nav.area.value).find(p => p.path === route.path)?.title ?? null)
</script>

<template>
    <div class="where" :class="{ searching }">
        <button type="button" class="crumb" @click="drawer.show">
            <span class="glyph">≡</span>
            <span class="section">{{ nav.activeSection.value?.label }}</span>
            <template v-if="page">
                <span class="sep">›</span>
                <span class="page">{{ page }}</span>
            </template>
        </button>
        <button v-if="!searching" type="button" class="find" aria-label="Search the docs" @click="searching = true">
            <home-pixel-icon :icon="ICONS.search" :size="14" />
        </button>
    </div>
</template>

<style scoped lang="scss">
// Only where the sidebar has gone into the drawer
.where {
    display: none;
}

@include tablet {
    // under the topnav, or under the search row while that's open
    .where {
        position: sticky;
        top: $topnav-height;
        z-index: 899;
        display: flex;
        gap: 0.75rem;
        align-items: center;
        padding: 0.4rem 1.25rem;
        background: rgba($night-sky, 0.72);
        backdrop-filter: blur(10px);
        border-bottom: 1px solid $glass-edge;

        &.searching {
            top: calc(#{$topnav-height} + #{$subnav-height});
        }
    }

    .crumb {
        flex: 1;
        display: flex;
        gap: 0.6rem;
        align-items: center;
        min-width: 0;
        padding: 0.25rem 0;
        font-family: $font-mono;
        font-size: 0.78rem;
        text-align: left;
        color: $ink-dim;
        background: none;
        border: 0;
        cursor: pointer;
    }

    .find {
        @include panel;
        flex: none;
        display: grid;
        place-items: center;
        width: 2.2rem;
        height: 2.2rem;
        color: $ink-faint;
        cursor: pointer;
    }

    .glyph {
        color: $primary-ink;
    }

    .section {
        font-family: $font-display;
        font-size: 0.62rem;
        text-transform: uppercase;
        color: $ink-faint;
    }

    .sep {
        color: $ink-faint;
    }

    .page {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        color: $ink;
    }
}
</style>
