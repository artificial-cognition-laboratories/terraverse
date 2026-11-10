<template>
    <div class="docs-layout">
        <shared-topnav />
        <docs-subnav />

        <div class="docs-container">
            <docs-sidebar :section="nav.activeSection.value" />
            <main class="docs-main">
                <slot />
            </main>
        </div>
    </div>
</template>

<script setup lang="ts">
const nav = useDocsNav()
</script>

<style scoped lang="scss">
.docs-layout {
    min-height: 100vh;
    min-height: 100dvh;
    padding-top: 50px;
    display: flex;
    flex-direction: column;
}

.docs-container {
    display: flex;
    align-items: flex-start;
    max-width: $site-width;
    width: 100%;
    margin: 0 auto;
    padding: 15px 2rem;
    gap: 2rem;
    flex: 1;
}

.docs-main {
    flex: 1;
    min-width: 0;
}

@include tablet {
    .docs-container {
        flex-direction: column;
        padding: 0 1.25rem;
        // In a column, flex-start would size the article to its content
        align-items: stretch;
    }
}

@include mobile {
    .docs-container {
        padding: 0 1rem;
    }
}
</style>
