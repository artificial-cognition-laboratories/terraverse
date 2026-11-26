<script setup lang="ts">
import { CATEGORY_LABELS, type Mod } from "~/registry"
import { format } from "~/lib/format"

/** A mod's particulars: its latest version, dates, game API, licence and where it's filed. */
defineProps<{ mod: Mod }>()
</script>

<template>
    <dl class="about">
        <div><dt>Latest</dt><dd>v{{ mod.latestVersion }}</dd></div>
        <div><dt>Updated</dt><dd>{{ format.date(mod.updatedAt) }}</dd></div>
        <div><dt>Published</dt><dd>{{ format.date(mod.createdAt) }}</dd></div>
        <div><dt>Game API</dt><dd>{{ mod.versions[0]?.api }}</dd></div>
        <div><dt>License</dt><dd>{{ mod.license }}</dd></div>
        <div>
            <dt>{{ mod.categories.length === 1 ? "Category" : "Categories" }}</dt>
            <dd class="filed">
                <NuxtLink v-for="c in mod.categories" :key="c" :to="{ path: '/registry/browse', query: { category: c } }">
                    {{ CATEGORY_LABELS[c] }}
                </NuxtLink>
            </dd>
        </div>
    </dl>
</template>

<style scoped lang="scss">
.about {
    display: grid;
    gap: 0.45rem;
    font-size: 0.75rem;

    div {
        display: flex;
        justify-content: space-between;
        gap: 1rem;
    }

    dt {
        color: $ink-faint;
    }

    dd {
        margin: 0;
        color: $ink-dim;
        text-align: right;
    }
}

.filed {
    display: flex;
    flex-wrap: wrap;
    justify-content: flex-end;
    gap: 0.2rem 0.6rem;

    a {
        color: $primary-ink;
        text-decoration: none;

        &:hover {
            color: $ink;
        }
    }
}
</style>
