<script setup lang="ts">
import { platforms } from "~/config/downloads"

/**
 * Every download as a block of glass with its icon: the visitor's platform first
 * and lit in the ocean, then the rest. A platform with nothing yet is shown, dim.
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
        if (!p.downloads.length) return [{ key: p.id, href: null, icon: p.icon, label: p.label, detail: "soon", lit: false, title: p.notReady ?? "" }]
        return p.downloads.map(d => ({
            key: d.href,
            href: d.href,
            icon: d.icon,
            // one download names its OS; several name themselves
            label: p.downloads.length === 1 ? p.label : d.label,
            detail: d.detail,
            lit: p === mine,
            title: `${p.label} · ${d.label}`,
        }))
    })
})
</script>

<template>
    <div class="tiles">
        <component
            :is="t.href ? 'a' : 'div'"
            v-for="t in tiles"
            :key="t.key"
            :href="t.href ?? undefined"
            class="tile"
            :class="{ lit: t.lit, soon: !t.href }"
            :title="t.title"
        >
            <Icon :name="t.icon" class="icon" />
            <span class="label">{{ t.label }}</span>
            <span class="detail">{{ t.detail }}</span>
        </component>
    </div>
</template>

<style scoped lang="scss">
.tiles {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
}

.tile {
    @include panel;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.3rem;
    width: 8.5rem;
    aspect-ratio: 1;
    color: $ink;
    text-decoration: none;
    transition: border-color 0.12s, background-color 0.12s, transform 0.12s;

    &:is(a):hover {
        border-color: rgba($primary-ink, 0.55);
        transform: translateY(-2px);

        .icon {
            color: $ink;
        }
    }

    // The visitor's own: glass tinted with the ocean, its edge lit
    &.lit {
        background-color: rgba($primary, 0.42);
        border-color: rgba($primary-ink, 0.55);

        .icon {
            color: #fff;
        }

        .detail {
            color: rgba(#fff, 0.65);
        }
    }

    &.soon > * {
        opacity: 0.4;
    }
}

.icon {
    font-size: 2.2rem;
    margin-bottom: 0.6rem;
    color: $ink-dim;
    transition: color 0.12s;
}

.label {
    font-family: $font-display;
    font-size: 0.72rem;
    text-transform: uppercase;
}

.detail {
    font-size: 0.62rem;
    color: $ink-faint;
    white-space: nowrap;
}

@include mobile {
    .tile {
        width: calc(50% - 0.375rem);
    }
}
</style>
