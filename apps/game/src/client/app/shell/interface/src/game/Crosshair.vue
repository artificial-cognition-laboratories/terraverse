<template>
    <div class="crosshair" />
</template>

<style scoped>
/* A 9×9 plus with a dark edge: the page can't invert the world behind it. */
.crosshair {
    position: absolute;
    left: 50%;
    top: 50%;
    width: calc(var(--u) * 9);
    height: calc(var(--u) * 9);
    transform: translate(-50%, -50%);
    background:
        linear-gradient(#fff, #fff) center / var(--u) 100% no-repeat,
        linear-gradient(#fff, #fff) center / 100% var(--u) no-repeat;
    filter: drop-shadow(0 0 var(--u) rgb(0 0 0 / 0.6));
    opacity: 0.9;
}
</style>
