<template>
    <!-- Below tablet this block moves into the topnav drawer; above it,
         `disabled` leaves it in place. One tree, two placements. -->
    <Teleport to="#mobile-page-nav" :disabled="!isMobileNav">
        <div class="docs-sidebar-block" :class="{ 'in-drawer': isMobileNav }">
            <div v-if="isMobileNav" class="drawer-tabs">
                <button
                    v-for="s in nav.sections.value"
                    :key="s.id"
                    type="button"
                    class="drawer-tab"
                    :class="{ active: s.id === section?.id }"
                    @click="nav.select(s)"
                >
                    {{ s.label }}
                </button>
            </div>

            <div ref="card" class="card">
                <nav v-for="group in section?.groups ?? []" :key="group.title" class="group" :aria-label="group.title">
                    <h3>{{ group.title }}</h3>
                    <ul>
                        <li v-for="item in group.items" :key="item.path ?? item.title">
                            <docs-sidebar-item :item="item" :depth="0" />
                        </li>
                    </ul>
                </nav>
            </div>
        </div>
    </Teleport>
</template>

<script setup lang="ts">
import type { DocsSection } from "~/config/docs-nav"

/** The docs' tree: the active section's groups, bare on the night (reading pages keep their chrome quiet). */
defineProps<{ section?: DocsSection }>()

const { isMobileNav } = useMobileNav()
const nav = useDocsNav()

// Keep the tree's scroll across page changes
const card = ref<HTMLElement | null>(null)
const savedScroll = useState("docs-sidebar-scroll", () => 0)
onBeforeUnmount(() => {
    if (card.value) savedScroll.value = card.value.scrollTop
})
onMounted(() => {
    if (card.value) card.value.scrollTop = savedScroll.value
})
</script>

<style scoped lang="scss">
// Sticks where the page first puts it (under both bars, $page-top down), so it
// never moves as the page scrolls; the tree scrolls itself when it's long
$top: calc(#{$topnav-height} + #{$subnav-height} + #{$page-top});

.docs-sidebar-block {
    position: sticky;
    top: $top;
}

.card {
    @include on-sky;
    display: grid;
    gap: 1.75rem;
    max-height: calc(100vh - #{$top} - 1.5rem);
    overflow-y: auto;
    overscroll-behavior: contain;
    scrollbar-width: thin;
    scrollbar-color: $night-4 transparent;
}

.group {
    h3 {
        margin: 0 0.65rem 0.6rem;
        font-family: $font-display;
        font-weight: 400;
        font-size: 0.6rem;
        text-transform: uppercase;
        letter-spacing: 0.06em;
        color: $ink-faint;
    }

    ul {
        display: grid;
        gap: 0.1rem;
        margin: 0;
        padding: 0;
        list-style: none;
    }
}

.drawer-tabs {
    display: flex;
    flex-wrap: wrap;
    gap: 0.4rem;
    margin-bottom: 1.25rem;
}

.drawer-tab {
    padding: 0.4rem 0.65rem;
    font-family: $font-display;
    font-size: 0.6rem;
    text-transform: uppercase;
    color: $ink-faint;
    background: transparent;
    border: 1px solid $glass-edge;
    cursor: pointer;

    &.active {
        color: $ink;
        background: rgba($primary-ink, 0.07);
        border-color: rgba($primary-ink, 0.55);
    }
}

// In the drawer the drawer scrolls, so the tree is a plain block
.docs-sidebar-block.in-drawer {
    position: static;

    .card {
        max-height: none;
        overflow: visible;
    }
}
</style>
