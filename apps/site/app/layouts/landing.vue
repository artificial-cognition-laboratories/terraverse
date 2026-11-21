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
    background: $stone-0;
    color: $ink;

    :deep(.topnav) {
        background: rgba($stone-0, 0.72);
        backdrop-filter: blur(10px);
        border-bottom: 1px solid rgba($ink, 0.06);
    }

    :deep(.footer) {
        background: $stone-0;
        border-top: 1px solid $stone-2;
    }
}
</style>
