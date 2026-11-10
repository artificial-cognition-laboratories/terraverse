<template>
    <!-- Below tablet this block moves into the topnav drawer; above it,
         `disabled` leaves it in place. One tree, two placements. -->
    <Teleport to="#mobile-page-nav" :disabled="!isMobileNav">
        <aside class="docs-sidebar-block" :class="{ 'in-drawer': isMobileNav }">
            <div v-if="isMobileNav" class="drawer-tabs">
                <button
                    v-for="s in nav.sections.value"
                    :key="s.id"
                    class="drawer-tab"
                    :class="{ active: s.id === section?.id }"
                    @click="nav.select(s)"
                >
                    {{ s.label }}
                </button>
            </div>

            <div ref="scrollEl" class="sidebar-scroll">
                <nav class="sidebar-nav">
                    <div v-for="group in section?.groups ?? []" :key="group.title" class="nav-section">
                        <h3 class="section-title">{{ group.title }}</h3>
                        <ul class="nav-items">
                            <li v-for="item in group.items" :key="item.path ?? item.title">
                                <docs-sidebar-item :item="item" :depth="0" />
                            </li>
                        </ul>
                    </div>

                    <div v-if="prev || next" class="sidebar-page-nav">
                        <NuxtLink v-if="prev" :to="prev.path" class="page-nav-btn">← {{ prev.label }}</NuxtLink>
                        <span v-else class="page-nav-btn page-nav-btn--disabled">←</span>
                        <NuxtLink v-if="next" :to="next.path" class="page-nav-btn page-nav-btn--next">
                            {{ next.label }} →
                        </NuxtLink>
                        <span v-else class="page-nav-btn page-nav-btn--disabled">→</span>
                    </div>
                </nav>
            </div>
        </aside>
    </Teleport>
</template>

<script setup lang="ts">
import type { DocsSection } from "~/config/docs-nav"

const props = defineProps<{ section?: DocsSection }>()

const { isMobileNav } = useMobileNav()
const nav = useDocsNav()

// Keep the tree's scroll across page changes
const scrollEl = ref<HTMLElement | null>(null)
const savedScroll = useState("docs-sidebar-scroll", () => 0)
onBeforeUnmount(() => {
    if (scrollEl.value) savedScroll.value = scrollEl.value.scrollTop
})
onMounted(() => {
    if (scrollEl.value) scrollEl.value.scrollTop = savedScroll.value
})

// Prev/next here step between sections; the article's own step between pages
const index = computed(() => nav.sections.value.findIndex(s => s.id === props.section?.id))
const prev = computed(() => (index.value > 0 ? nav.sections.value[index.value - 1] : undefined))
const next = computed(() =>
    index.value !== -1 && index.value < nav.sections.value.length - 1 ? nav.sections.value[index.value + 1] : undefined,
)
</script>

<style scoped lang="scss">
// Two fixed bars sit above the docs: the topnav (52px) and the subnav (~37px),
// plus a little breathing room. `max-height`, not `height`, or sticky has no
// range to stick within.
$docs-sidebar-top: 100px;

.docs-sidebar-block {
    width: 220px;
    flex-shrink: 0;
    position: sticky;
    top: $docs-sidebar-top;
    max-height: calc(100vh - #{$docs-sidebar-top} - 1rem);
    display: flex;
    flex-direction: column;
    padding-top: 0.75rem;
}

.sidebar-scroll {
    flex: 1;
    overflow-y: auto;
    overflow-x: hidden;
    min-height: 0;
    scrollbar-width: thin;
    scrollbar-color: rgba($primary, 0.2) transparent;
}

.sidebar-nav {
    display: flex;
    flex-direction: column;
    gap: 2rem;
}

.section-title {
    font-size: 0.75rem;
    font-weight: 600;
    color: $text-tertiary;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    margin: 0 0 0.75rem;
}

.nav-items {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
}

.sidebar-page-nav {
    display: flex;
    gap: 0.5rem;
}

.page-nav-btn {
    flex: 1;
    min-width: 0;
    font-size: 0.75rem;
    color: $text-tertiary;
    border: 1px solid rgba($primary, 0.2);
    padding: 0.3rem 0.5rem;
    text-decoration: none;
    line-height: 1;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;

    &:hover {
        color: $primary;
        border-color: rgba($primary, 0.5);
    }

    &--next {
        text-align: right;
    }

    &--disabled {
        opacity: 0.2;
    }
}

.drawer-tabs {
    display: flex;
    flex-wrap: wrap;
    gap: 0.4rem;
    margin-bottom: 1.5rem;
}

.drawer-tab {
    font-size: 0.78rem;
    color: $text-tertiary;
    background: transparent;
    border: 1px solid $border-mid;
    padding: 0.35rem 0.6rem;
    cursor: pointer;

    &.active {
        color: $primary;
        border-color: $border-strong;
        background: rgba($primary, 0.08);
    }
}

// In the drawer the drawer scrolls, so the tree is a plain block
.docs-sidebar-block.in-drawer {
    width: 100%;
    position: static;
    max-height: none;

    .sidebar-scroll {
        overflow: visible;
    }
}
</style>
