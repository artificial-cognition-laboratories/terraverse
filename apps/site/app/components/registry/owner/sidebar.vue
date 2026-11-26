<script setup lang="ts">
import { CATEGORY_LABELS, type Owner } from "~/registry"
import { format } from "~/lib/format"

/** An owner page's side card, as a mod's: what to look at, their numbers, then where their work is filed. */
const props = defineProps<{ owner: Owner; stats: readonly { icon: "mod" | "star" | "download"; value: string; label: string }[] }>()

const contents = computed(() => [{ id: "overview" as const, label: "Overview", icon: "mod" as const, count: props.owner.listings.length }])
// the newest change across everything they've published, and every category it's filed under
const updated = computed(() => props.owner.listings.map(l => l.updatedAt).sort().at(-1) ?? null)
const categories = computed(() => [...new Set(props.owner.listings.flatMap(l => l.categories))])
</script>

<template>
    <shared-side class="sidebar">
        <shared-side-nav :items="contents" active="overview" />
        <shared-side-stats :items="stats" />
        <dl class="meta">
            <div><dt>Updated</dt><dd>{{ updated ? format.date(updated) : "—" }}</dd></div>
            <div>
                <dt>{{ categories.length === 1 ? "Category" : "Categories" }}</dt>
                <dd class="filed">
                    <NuxtLink v-for="c in categories" :key="c" :to="{ path: '/registry/browse', query: { category: c } }">
                        {{ CATEGORY_LABELS[c] }}
                    </NuxtLink>
                </dd>
            </div>
        </dl>
    </shared-side>
</template>

<style scoped lang="scss">
.meta {
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
