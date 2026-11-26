<script setup lang="ts">
import { RAIL_BRUSHES, type BrushId, type StateSections } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import IconButton from "../kit/IconButton.vue"
import PixelIcon from "./PixelIcon.vue"
import { BRUSH_ICONS, INK } from "./toolIcons.ts"

/**
 * The tools, low on the left while building (Tab, sprint 70): Add, Cut and Paint; R steps through them. The shape
 * they draw is on the bar over the hotbar (ShapeBar). Add makes objects; held, Alt adds to the world, and the badge
 * says so. Joints, and what's done with a part, are on its menu in the world (right-click it).
 */
// free: the mouse is (C, or paused)
const props = defineProps<{ tools: Extract<StateSections["tools"], { out: true }>; free: boolean }>()
const bridge = useBridge()
const brush = (id: BrushId) => {
    if (props.free) void bridge.send("tools.brush", { brush: id })
}
</script>

<template>
    <nav class="tool-rail rail" :class="{ free: free }" aria-label="Tool">
        <div class="way" :class="{ world: !tools.creating }">{{ tools.creating ? "Build" : "World" }}</div>
        <IconButton v-for="id in RAIL_BRUSHES" :key="id" :label="BRUSH_ICONS[id].label"
            :on="tools.brush === id" side="right" :tabindex="free ? 0 : -1" @click="brush(id)">
            <PixelIcon :rows="BRUSH_ICONS[id].rows" :palette="INK" style="--u: 2px" />
        </IconButton>
    </nav>
</template>

<style scoped>
/* low on the left, a third of the screen at most, wrapping into columns to the right */
.tool-rail {
    position: absolute; left: 20px; bottom: 110px;
    display: flex; flex-direction: column; flex-wrap: wrap; align-content: flex-start;
    gap: 10px; max-height: 33vh;
}
/* the mouse is free only with C, or paused: elsewhere the rail is shown, not touched */
.tool-rail.free { pointer-events: auto; }
.way { width: 48px; padding: 3px 0; text-align: center; font: 9px var(--font-display); text-transform: uppercase;
    color: var(--ink); background: var(--stone-1); border: var(--b) solid var(--edge); border-radius: var(--radius);
    box-shadow: inset 0 -3px 0 var(--accent); }
.way.world { box-shadow: inset 0 -3px 0 var(--ink-faint); }
</style>
