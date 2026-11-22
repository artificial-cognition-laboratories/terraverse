<script setup lang="ts">
import { ICONS, type CategoryId } from "~/config/icons"
import { LISTING_TYPES, TYPE_ICONS, TYPE_LABELS, type Category, type ListingType, type Search, type Totals } from "~/registry"
import { format } from "~/lib/format"

/**
 * Browse's side card: what kind of thing, the catalogue in numbers, which
 * categories, official only. Emits the search changed.
 */
const props = defineProps<{ search: Search; categories: readonly Category[]; totals: Totals }>()
const emit = defineEmits<{ change: [search: Partial<Search>] }>()

type Kind = ListingType | "all"
const types = computed(() => [
    { id: "all" as const, label: "Everything", icon: TYPE_ICONS.all, count: props.totals.listings },
    ...LISTING_TYPES.map(t => ({ id: t, label: TYPE_LABELS[t], icon: TYPE_ICONS[t], count: props.totals.types[t] })),
])
const stats = computed(() => [
    { icon: "all" as const, value: format.count(props.totals.listings), label: "listings" },
    { icon: "download" as const, value: format.count(props.totals.installs), label: "installs" },
    { icon: "author" as const, value: format.count(props.totals.authors), label: "authors" },
])

function pick(kind: Kind) {
    emit("change", { type: kind === "all" ? null : kind })
}
function toggle(id: CategoryId) {
    const has = props.search.categories.includes(id)
    emit("change", { categories: has ? props.search.categories.filter(c => c !== id) : [...props.search.categories, id] })
}
</script>

<template>
    <shared-side class="filters">
        <shared-side-nav :items="types" :active="search.type ?? 'all'" @pick="pick" />
        <shared-side-stats :items="stats" />

        <section>
            <h4>Categories</h4>
            <ul class="categories">
                <li v-for="c in categories" :key="c.id">
                    <button type="button" :class="{ on: search.categories.includes(c.id) }" :aria-pressed="search.categories.includes(c.id)" @click="toggle(c.id)">
                        <span class="box" aria-hidden="true" />
                        <home-pixel-icon :icon="ICONS[c.id]" :size="16" />
                        <span class="name">{{ c.label }}</span>
                        <span class="count">{{ c.count }}</span>
                    </button>
                </li>
            </ul>
        </section>

        <section>
            <button type="button" class="switch" :class="{ on: search.official }" :aria-pressed="search.official" @click="emit('change', { official: !search.official })">
                <span class="name">Official only</span>
                <span class="track" aria-hidden="true"><span class="knob" /></span>
            </button>
        </section>
    </shared-side>
</template>

<style scoped lang="scss">
h4 {
    margin: 0 0 0.75rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.62rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $ink-faint;
}

button {
    font-family: inherit;
    color: inherit;
    background: none;
    border: none;
    cursor: pointer;
}

.categories {
    display: grid;
    gap: 0.15rem;
    margin: 0;
    padding: 0;
    list-style: none;

    button {
        display: flex;
        gap: 0.7rem;
        align-items: center;
        width: 100%;
        padding: 0.45rem 0.25rem;
        text-align: left;

        &:hover .name {
            color: $ink;
        }

        &.on {
            .box {
                background: $primary;
                border-color: $primary-ink;
                box-shadow: inset 0 0 0 2px $night-sky;
            }

            .name {
                color: $ink;
            }
        }
    }
}

.box {
    flex: none;
    width: 0.8rem;
    height: 0.8rem;
    border: 1px solid $night-4;
}

.name {
    flex: 1;
    font-family: $font-sans;
    font-size: 0.88rem;
    color: $ink-dim;
}

.count {
    font-size: 0.7rem;
    color: $ink-faint;
}

.switch {
    text-align: left;
    display: flex;
    align-items: center;
    gap: 0.75rem;
    width: 100%;
    padding: 0.25rem;

    .track {
        position: relative;
        width: 2.2rem;
        height: 1.1rem;
        border: 1px solid $night-4;
        transition: background 0.12s, border-color 0.12s;
    }

    .knob {
        position: absolute;
        top: 2px;
        left: 2px;
        width: calc(1.1rem - 6px);
        height: calc(1.1rem - 6px);
        background: $ink-faint;
        transition: transform 0.12s, background 0.12s;
    }

    &.on {
        .track {
            background: rgba($primary, 0.5);
            border-color: $primary-ink;
        }

        .knob {
            background: #fff;
            transform: translateX(1.1rem);
        }

        .name {
            color: $ink;
        }
    }
}
</style>
