<script setup lang="ts">
/** Under the topnav on every docs page: each section of the docs, and their search. */
const nav = useDocsNav()
const links = computed(() =>
    nav.sections.value.map(s => ({ label: s.label, to: s.path, on: nav.activeSection.value?.id === s.id })),
)
</script>

<template>
    <shared-subnav :links="links" class="docs-subnav">
        <docs-search />
    </shared-subnav>
</template>

<style scoped lang="scss">
// Below tablet the sections live in the drawer, as tabs above the tree; the search stays
@include tablet {
    .docs-subnav :deep(.links) {
        display: none;
    }

    .docs-subnav :deep(.search) {
        flex: 1;
        width: auto;
    }
}
</style>
