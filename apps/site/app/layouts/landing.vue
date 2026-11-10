<template>
    <div class="landing-shell">
        <shared-topnav />
        <main>
            <slot />
        </main>
    </div>
</template>

<style scoped lang="scss">
// 52px clears the fixed topnav
.landing-shell {
    min-height: 100vh;
    min-height: 100dvh;
    padding-top: 52px;
    background: $bg-space;

    // The nav sits on the same sky as the hero, not the docs' grey
    :deep(.topnav) {
        background: $bg-space;
    }
}
</style>
