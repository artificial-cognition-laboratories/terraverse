<script setup lang="ts">
/** A short list of claims, each a bold line and a sentence under it. */
defineProps<{ points: readonly { title: string; text: string }[] }>()
</script>

<template>
    <ul class="points">
        <li v-for="p in points" :key="p.title">
            <strong>{{ p.title }}</strong>
            <span>{{ p.text }}</span>
        </li>
    </ul>
</template>

<style scoped lang="scss">
.points {
    @include on-sky;
    display: grid;
    gap: 1.25rem;
    margin: 0;
    padding: 0;
    list-style: none;
}

li {
    display: grid;
    gap: 0.3rem;
    padding-left: 1.4rem;
    position: relative;
    font-family: $font-sans;

    &::before {
        content: "";
        position: absolute;
        left: 0;
        top: 0.45em;
        width: 0.5rem;
        height: 0.5rem;
        background: $primary-ink;
    }
}

strong {
    font-size: 1rem;
    color: $ink;
}

span {
    font-size: 0.92rem;
    line-height: 1.55;
    color: $ink-dim;
}
</style>
