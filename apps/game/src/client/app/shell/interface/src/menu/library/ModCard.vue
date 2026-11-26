<script setup lang="ts">
import { computed } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { useCache } from "../../cache/index.ts"
import { Card } from "../../kit/index.ts"
import { format } from "./format.ts"

/** A registry listing as a card. */
const props = withDefaults(defineProps<{
    listing: RegistryListing; row?: boolean
    /** One you're making, from your profile's mods. */ yours?: boolean
    /** The label of its first category, for the tag in its corner. */ tag?: string | null
}>(), { row: false, yours: false, tag: null })
defineEmits<{ open: [] }>()
// a pointer resting on a card is about to open it: its page is fetched ahead, so it opens from the cache
const HOVER_MS = 100
const cache = useCache()
let hover: ReturnType<typeof setTimeout> | undefined
function rest() {
    if (props.yours) return
    hover = setTimeout(() => cache.prefetch("registry.mod", { name: props.listing.name }), HOVER_MS)
}
const leave = () => clearTimeout(hover)
const badges = computed(() => [
    ...(props.yours ? ["Yours"] : []),
    ...(props.listing.official ? ["Official"] : []),
    ...(props.listing.installed && !props.yours ? ["Installed"] : []),
    ...(props.listing.type === "textures" ? ["Textures"] : []),
])
</script>

<template>
    <Card :title="listing.title ?? format.bare(listing.name)" :seed="listing.name" :by="listing.owner"
        :description="listing.description" :image="listing.banner" :icon="listing.icon" :badges="badges" :row="row"
        @open="$emit('open')" @pointerenter="rest" @pointerleave="leave">
        <template #meta>
            <span title="Stars">★ {{ format.count(listing.stars) }}</span>
            <span v-if="listing.installs !== null" title="Installs">↓ {{ format.count(listing.installs) }}</span>
            <template v-if="row"><span>v{{ listing.latestVersion }}</span><span>{{ format.ago(listing.updatedAt) }}</span></template>
            <span v-if="tag" class="tag">{{ tag }}</span>
        </template>
    </Card>
</template>
