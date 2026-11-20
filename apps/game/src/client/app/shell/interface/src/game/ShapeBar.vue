<script setup lang="ts">
import { computed } from "vue"
import { TOOL_IDS, type StateSections, type ToolId } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import PixelIcon from "./PixelIcon.vue"
import { INK, TOOL_ICONS } from "./toolIcons.ts"

/**
 * The shapes, on the hotbar while building (sprint 70): the slot keys pick them, as they pick items otherwise (Alt
 * and a number: the item). The wheel sizes the shape; its size is at the end. Clicked where the mouse is free.
 */
const props = defineProps<{ tools: Extract<StateSections["tools"], { out: true }>; free: boolean }>()
const bridge = useBridge()
const pick = (id: ToolId) => {
    if (props.free) void bridge.send("tools.pick", { tool: id })
}
const SIXTEENTHS = 16
const size = computed(() =>
    props.tools.size % SIXTEENTHS === 0 ? `${props.tools.size / SIXTEENTHS} m` : `${props.tools.size}/16`)
</script>

<template>
    <div class="shape-bar" :class="{ free: free }" aria-label="Shape">
        <button v-for="(id, i) in TOOL_IDS" :key="id" class="cell" :class="{ on: tools.tool === id }"
            :title="TOOL_ICONS[id].label" :tabindex="free ? 0 : -1" @click="pick(id)">
            <PixelIcon :rows="TOOL_ICONS[id].rows" :palette="INK" style="--u: 2px" />
            <span class="key">{{ i + 1 }}</span>
        </button>
        <span class="size" title="Size: the mouse wheel">{{ size }}</span>
    </div>
</template>

<style scoped>
/* as the hotbar's cells, so a number reads as the same slot */
.shape-bar {
    display: flex; align-items: center; align-self: center; gap: 0;
    padding: var(--u); background: rgb(20 20 20 / 0.55);
    border: var(--u) solid #1b1b1b; box-shadow: inset 0 0 0 var(--u) #5a5a5a;
}
.shape-bar.free { pointer-events: auto; }
.cell {
    position: relative; display: grid; place-items: center;
    width: calc(var(--u) * 16); height: calc(var(--u) * 16); padding: 0;
    color: var(--ink); background: none; border: 0;
    box-shadow: inset 0 0 0 var(--u) rgb(139 139 139 / 0.35);
}
.shape-bar.free .cell { cursor: pointer; }
.cell.on { background: var(--accent-lo); box-shadow: inset 0 calc(var(--u) * -1) 0 var(--accent), inset 0 0 0 var(--u) var(--accent); }
.key { position: absolute; left: 2px; top: 1px; font: 9px var(--font-display); color: var(--ink-faint); }
.cell.on .key { color: var(--ink); }
.size { padding: 0 calc(var(--u) * 3); font: 11px var(--font-display); color: var(--ink); white-space: nowrap; }
</style>
