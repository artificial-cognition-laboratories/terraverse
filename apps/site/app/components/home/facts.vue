<script setup lang="ts">
import { facts } from "~/config/home"
</script>

<template>
    <ul class="facts">
        <li v-for="fact in facts" :key="fact">{{ fact }}</li>
    </ul>
</template>

<style scoped lang="scss">
.facts {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 0.75rem 2.5rem;
    margin: 0;
    padding: 1.4rem 2rem;
    list-style: none;
    background: $stone-1;
    border-top: 1px solid $stone-3;
    border-bottom: 1px solid $stone-3;
}

li {
    font-family: $font-display;
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $ink-dim;
    white-space: nowrap;

    &::before {
        content: "■";
        margin-right: 0.6rem;
        color: $primary-ink;
        font-size: 0.55rem;
        vertical-align: 0.1em;
    }
}
</style>
