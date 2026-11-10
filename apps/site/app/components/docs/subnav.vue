<template>
    <div class="docs-subnav">
        <div class="subnav-container">
            <div v-for="side in sides" :key="side.name" class="subnav-tabs" :class="`subnav-tabs-${side.name}`">
                <button
                    v-for="section in side.sections"
                    :key="section.id"
                    class="subnav-tab"
                    :class="{ active: nav.activeSection.value?.id === section.id }"
                    @click="nav.select(section)"
                >
                    {{ section.label }}
                </button>
            </div>
        </div>
    </div>
</template>

<script setup lang="ts">
const nav = useDocsNav()

const sides = computed(() => [
    { name: "left", sections: nav.sections.value.filter(s => s.side !== "right") },
    { name: "right", sections: nav.sections.value.filter(s => s.side === "right") },
])
</script>

<style scoped lang="scss">
.docs-subnav {
    position: fixed;
    top: 52px; // below the fixed topnav
    left: 0;
    right: 0;
    z-index: 900;
    background: $bg-primary;
}

.subnav-container {
    max-width: $site-width;
    margin: 0 auto;
    padding: 0 2rem;
    display: flex;
    justify-content: space-between;
    width: 100%;
}

.subnav-tabs {
    display: flex;
    gap: 1.25rem;
    overflow-x: auto;
    scrollbar-width: none;
    min-width: 0;
}

.subnav-tabs-right {
    flex: none;
    margin-left: 1.25rem;
}

.subnav-tab {
    font-size: 0.8rem;
    color: $text-tertiary;
    background: $bg-primary;
    border: none;
    border-bottom: 1px solid transparent;
    padding: 0 0 0.5rem;
    cursor: pointer;
    white-space: nowrap;

    &:hover {
        color: $text-secondary;
    }

    &.active {
        color: $primary;
        border-bottom-color: $primary;
    }
}

// Below tablet the sections live in the drawer, as tabs above the tree
@include tablet {
    .docs-subnav {
        display: none;
    }
}
</style>
