<template>
    <div class="landing-shell">
        <shared-topnav />
        <main>
            <slot />
        </main>
    </div>
</template>

<style scoped lang="scss">
// The hero runs up under the topnav, so the sky starts at the top of the window
.landing-shell {
    min-height: 100dvh;

    // The ground holds the footer: it sits on the earth, not a band of its own
    :deep(.footer) {
        border-top: none;
    }
}
</style>
