<script setup lang="ts">
import { computed } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { Card } from "../../kit/index.ts"
import { format } from "./format.ts"

/** A registry listing as a card. */
const props = withDefaults(defineProps<{ listing: RegistryListing; dense?: boolean }>(), { dense: false })
defineEmits<{ open: [] }>()
const badges = computed(() => [
    ...(props.listing.official ? ["Official"] : []),
    ...(props.listing.installed ? ["Installed"] : []),
    ...(props.listing.type === "textures" ? ["Textures"] : []),
])
</script>

<template>
    <Card :title="listing.title ?? format.bare(listing.name)" :seed="listing.name" :by="listing.owner"
        :description="listing.description" :image="listing.banner" :icon="listing.icon" :badges="badges" :dense="dense"
        @open="$emit('open')">
        <template #meta>
            <span title="Stars">★ {{ format.count(listing.stars) }}</span>
            <span v-if="listing.installs !== null" title="Installs">↓ {{ format.count(listing.installs) }}</span>
            <span>v{{ listing.latestVersion }}</span>
            <span class="when">{{ format.ago(listing.updatedAt) }}</span>
        </template>
    </Card>
</template>

<style scoped>
.when { margin-left: auto; }
</style>
