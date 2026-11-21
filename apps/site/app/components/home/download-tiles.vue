<script setup lang="ts">
import { platforms } from "~/config/downloads"

/**
 * Every download as a large tile: the visitor's platform first and lit in the
 * brand's deep ocean, then the rest. A platform with nothing to download yet is shown, dimmed.
 */
const { detected } = usePlatform()

interface Tile {
    key: string
    /** null while there's nothing to download */
    href: string | null
    icon: string
    label: string
    detail: string
    lit: boolean
    title: string
}

const tiles = computed<Tile[]>(() => {
    const mine = platforms.find(p => p.id === detected.value)
    const ordered = mine ? [mine, ...platforms.filter(p => p !== mine)] : platforms
    return ordered.flatMap((p): Tile[] => {
        const lit = p === mine
        if (!p.downloads.length) return [{ key: p.id, href: null, icon: p.icon, label: p.label, detail: "soon", lit: false, title: p.notReady ?? "" }]
        return p.downloads.map(d => ({
            key: d.href,
            href: d.href,
            icon: d.icon,
            // one download names its OS; several name themselves
            label: p.downloads.length === 1 ? p.label : d.label,
            detail: d.detail,
            lit: lit,
            title: `${p.label} · ${d.label}`,
        }))
    })
})
</script>

<template>
    <div class="tiles">
        <template v-for="t in tiles" :key="t.key">
            <a v-if="t.href" :href="t.href" class="tile" :class="{ lit: t.lit }" :title="t.title">
                <Icon :name="t.icon" class="icon" />
                <span class="label">{{ t.label }}</span>
                <span class="detail">{{ t.detail }}</span>
            </a>
            <div v-else class="tile soon" :title="t.title">
                <Icon :name="t.icon" class="icon" />
                <span class="label">{{ t.label }}</span>
                <span class="detail">{{ t.detail }}</span>
            </div>
        </template>
    </div>
</template>

<style scoped lang="scss">
@use "sass:color";

.tiles {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
}

.tile {
    @include panel;
    position: relative;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.35rem;
    width: 7.5rem;
    aspect-ratio: 1;
    color: $ink;
    text-decoration: none;
    transition: border-color 0.1s, background 0.1s;

    &:hover {
        border-color: $primary-ink;
        background: $night-2;

        .icon {
            color: $primary-ink;
        }
    }

    // The visitor's own: the deep ocean, lit like a button
    &.lit {
        color: #fff;
        background: $primary;
        border-color: $edge;
        box-shadow: $raised;

        .detail {
            color: rgba(#fff, 0.7);
        }

        &:hover {
            background: color.adjust($primary, $lightness: 6%);

            .icon {
                color: #fff;
            }
        }
    }

    // dim what's on it, not the tile: the sky mustn't show through
    &.soon {
        cursor: default;

        > * {
            opacity: 0.45;
        }


        &:hover {
            border-color: $night-3;
            background: $night-1;

            .icon {
                color: inherit;
            }
        }
    }
}

.icon {
    font-size: 2.1rem;
    margin-bottom: 0.4rem;
    transition: color 0.1s;
}

.label {
    font-family: $font-display;
    font-size: 0.72rem;
    text-transform: uppercase;
}

.detail {
    font-size: 0.6rem;
    color: $ink-faint;
    white-space: nowrap;
}

@include mobile {
    .tile {
        width: calc(50% - 0.375rem);
    }
}
</style>
