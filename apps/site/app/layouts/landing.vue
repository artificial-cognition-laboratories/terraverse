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
    background: $night-sky;
    color: $ink;

    :deep(.topnav) {
        background: rgba($night-0, 0.72);
        backdrop-filter: blur(10px);
        border-bottom: 1px solid rgba($ink, 0.06);
    }

    :deep(.footer) {
        background: transparent;
        border-top: none;
    }
}
</style>
