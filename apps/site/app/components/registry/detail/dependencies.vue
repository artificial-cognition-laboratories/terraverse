<script setup lang="ts">
import type { Mod } from "~/registry"

/** What this version needs, and what needs it: each one a link to its own page. */
const props = defineProps<{ mod: Mod; version: string }>()
const needs = computed(() => Object.entries(props.mod.versions.find(v => v.version === props.version)?.dependencies ?? {}))
</script>

<template>
    <div class="dependencies">
        <section>
            <h3>Needs <span>{{ needs.length }}</span></h3>
            <ul v-if="needs.length">
                <li v-for="[name, range] in needs" :key="name">
                    <NuxtLink :to="`/registry/${name}`">{{ name }}</NuxtLink>
                    <code>{{ range }}</code>
                </li>
            </ul>
            <p v-else>Nothing: it runs on the game alone.</p>
        </section>
        <section>
            <h3>Needed by <span>{{ mod.dependents.length }}</span></h3>
            <ul v-if="mod.dependents.length">
                <li v-for="name in mod.dependents" :key="name">
                    <NuxtLink :to="`/registry/${name}`">{{ name }}</NuxtLink>
                </li>
            </ul>
            <p v-else>Nothing yet. Build on it.</p>
        </section>
    </div>
</template>

<style scoped lang="scss">
.dependencies {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1rem;
}

section {
    @include panel;
    padding: 1.25rem 1.5rem;
}

h3 {
    margin: 0 0 1rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.68rem;
    text-transform: uppercase;
    color: $ink;

    span {
        margin-left: 0.4rem;
        color: $ink-faint;
    }
}

ul {
    display: grid;
    gap: 0.5rem;
    margin: 0;
    padding: 0;
    list-style: none;
}

li {
    display: flex;
    justify-content: space-between;
    gap: 1rem;
    padding: 0.6rem 0.8rem;
    font-family: $font-mono;
    font-size: 0.8rem;
    border: 1px solid $glass-edge;

    a {
        color: $primary-ink;
        text-decoration: none;

        &:hover {
            color: $ink;
        }
    }

    code {
        color: $ink-faint;
    }
}

p {
    margin: 0;
    font-size: 0.85rem;
    color: $ink-faint;
}

@include tablet {
    .dependencies {
        grid-template-columns: minmax(0, 1fr);
    }
}
</style>
