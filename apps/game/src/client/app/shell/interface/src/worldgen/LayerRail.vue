<script setup lang="ts">
import { computed } from "vue"
import PixelIcon from "../game/PixelIcon.vue"
import { IconTile } from "../kit/index.ts"
import { OVERLAYS, type OverlayId } from "./fields.ts"
import { OVERLAY_ICONS } from "./overlayIcons.ts"

/**
 * A planet's layers to paint over it, two columns of icon tiles: the
 * builder's and the map's. `ids` are the ones offered (all, by default);
 * the slot takes a screen's own tiles after them (seams, the cutaway).
 */
const props = defineProps<{ ids?: readonly OverlayId[] }>()
const chosen = defineModel<OverlayId | string>({ required: true })
const offered = computed(() => props.ids ? OVERLAYS.filter((o) => props.ids!.includes(o.id)) : OVERLAYS)
</script>

<template>
    <nav class="layers" aria-label="Planet layers">
        <IconTile v-for="item in offered" :key="item.id" :label="item.label" side="left" :size="36"
            :on="chosen === item.id" @click="chosen = item.id">
            <PixelIcon :rows="OVERLAY_ICONS[item.id].rows" :palette="OVERLAY_ICONS[item.id].palette" style="--u: 2px" />
        </IconTile>
        <slot />
    </nav>
</template>

<style scoped>
.layers { display: grid; grid-template-columns: repeat(2, 36px); align-content: start; gap: 6px; }
</style>
