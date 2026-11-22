<script setup lang="ts">
import { computed } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { Card } from "../../kit/index.ts"
import { format } from "./format.ts"

/** A registry listing as a card. */
const props = withDefaults(defineProps<{
    listing: RegistryListing; row?: boolean
    /** One you're making, from your profile's mods. */ yours?: boolean
    /** The label of its first category, for the tag in its corner. */ tag?: string | null
}>(), { row: false, yours: false, tag: null })
defineEmits<{ open: [] }>()
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
        @open="$emit('open')">
        <template #meta>
            <span title="Stars">★ {{ format.count(listing.stars) }}</span>
            <span v-if="listing.installs !== null" title="Installs">↓ {{ format.count(listing.installs) }}</span>
            <template v-if="row"><span>v{{ listing.latestVersion }}</span><span>{{ format.ago(listing.updatedAt) }}</span></template>
            <span v-if="tag" class="tag">{{ tag }}</span>
        </template>
    </Card>
</template>
