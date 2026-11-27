<script setup lang="ts">
import { computed } from "vue"
import type { RegistryMod } from "../../bridge/index.ts"
import { DetailHeader } from "../../kit/index.ts"
import PixelIcon from "../../game/PixelIcon.vue"
import { ICONS } from "../../kit/icons.ts"
import { format } from "./format.ts"

/**
 * The top of a mod's page, as the website's (registry/detail/header.vue): the kit's detail header
 * with the address copying the line that installs it, the version and whether it's official or
 * yours, and the star beside what you do with it (the `actions` slot: install, or reload your own).
 */
const props = withDefaults(defineProps<{ mod: RegistryMod; yours?: boolean }>(), { yours: false })
const title = computed(() => props.mod.title ?? format.bare(props.mod.name))
</script>

<template>
    <DetailHeader :seed="mod.name" :title="title" :banner="mod.banner" :icon="mod.icon" :address="mod.name"
        :copies="`terra install ${mod.name}`" :description="mod.description">
        <template #badges>
            <span class="badge">v{{ mod.latestVersion }}</span>
            <span v-if="mod.official" class="badge lit">Official</span>
            <span v-if="yours" class="badge lit">Yours</span>
        </template>
        <template #actions>
            <!-- starring isn't on the bridge yet: the count shows, the press waits -->
            <button v-if="!yours" type="button" class="star" disabled title="Starring isn't wired yet">
                <PixelIcon :rows="ICONS.star.rows" :palette="ICONS.star.palette" style="--u: 1.6px" />
                <span>Star</span>
                <span class="count">{{ format.count(mod.stars) }}</span>
            </button>
            <slot name="actions" />
        </template>
        <template #by>by <b>{{ yours ? "you" : mod.owner }}</b></template>
        <p v-if="mod.deprecated !== null" class="deprecated">Deprecated: {{ mod.deprecated || "its author no longer keeps it." }}</p>
    </DetailHeader>
</template>

<style scoped>
.star { display: inline-flex; align-items: center; gap: 0.55rem; height: 2.1rem; padding: 0 0.75rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); color: var(--ink-dim); font: 0.62rem var(--font-display); text-transform: uppercase; }
.star:disabled { cursor: default; }
.star svg { filter: grayscale(1) brightness(0.8); }
.star .count { padding-left: 0.55rem; border-left: 1px solid var(--glass-edge); color: var(--ink-faint); font: 0.72rem var(--font); }
.deprecated { margin: 0; padding: 0.75rem 1rem; border: 1px solid rgb(210 153 34 / 0.5); border-radius: var(--radius); color: var(--warn); font-size: 0.85rem; }
</style>
