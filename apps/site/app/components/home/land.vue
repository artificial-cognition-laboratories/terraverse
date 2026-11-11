<template>
    <section class="land">
        <img src="/land/relief.webp" alt="" class="bg" width="768" height="768" loading="lazy" decoding="async" />
        <div class="inner">
            <p class="kick">» your planet is waiting</p>
            <h2>Pick a code.<br /><em>Land.</em></h2>
            <home-download />
        </div>
    </section>
</template>

<style scoped lang="scss">
.land {
    position: relative;
    overflow: hidden;
    padding: 8rem 2rem;
    border-top: 1px solid $border-soft;
    background: $bg-space;
}

.bg {
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    object-fit: cover;
    image-rendering: pixelated;
    opacity: 0.28;
    mask-image: linear-gradient(to bottom, transparent, #000 40%, #000 70%, transparent);
}

.inner {
    position: relative;
    max-width: 38rem;
    margin: 0 auto;
    display: grid;
    // A bare grid column grows to its widest child; pin it to the container
    grid-template-columns: minmax(0, 1fr);
    justify-items: center;
    text-align: center;
}

.kick {
    margin: 0;
    font-size: 0.75rem;
    color: $primary;
    text-transform: uppercase;
    letter-spacing: 0.08em;
}

h2 {
    font-family: $font-display;
    font-weight: 400;
    font-size: clamp(2rem, 6vw, 3.5rem);
    line-height: 1.1;
    color: #e8eef4;
    margin: 1rem 0 2.5rem;

    em {
        font-style: normal;
        color: $primary;
    }
}

.inner :deep(.download) {
    width: 100%;
    text-align: left;
}

@include mobile {
    .land {
        padding: 5rem 1.25rem;
    }
}
</style>
