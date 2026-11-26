<script setup lang="ts">
import { inject, onBeforeUnmount, onMounted, ref } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { sceneRect } from "../../wiki/sceneRect.ts"
import { HOLE } from "./hole.ts"

/**
 * The player's figure, standing in this box: the engine draws it natively under the page
 * (src/shell/bridge/figure.h), and the screen around it leaves a hole for it (HOLE). Dragging
 * across it turns it; the engine takes that, as the page draws nothing there.
 */
const bridge = useBridge()
const hole = inject(HOLE, null)
const view = ref<HTMLElement | null>(null)

function measure() {
    if (hole) hole.value = view.value?.getBoundingClientRect() ?? null
    return sceneRect(view.value)
}
const fail = (error: unknown) => console.error("figure", error)
// the box moves with the window and the page's scale, not only with its own size
const sizes = new ResizeObserver(() => {
    const rect = measure()
    if (rect) bridge.send("figure.place", { rect: rect }).catch(fail)
})
onMounted(() => {
    const rect = measure()
    if (rect) bridge.send("figure.show", { rect: rect }).catch(fail)
    if (view.value) sizes.observe(view.value)
    sizes.observe(document.documentElement)
})
onBeforeUnmount(() => {
    sizes.disconnect()
    if (hole) hole.value = null
    bridge.send("figure.hide", {}).catch(fail)
})
</script>

<template>
    <div class="figure-well"><div ref="view" class="view" /></div>
</template>

<style scoped>
/* a hairline round it, as round the crafting; inside, the engine's own night and the figure */
.figure-well { display: flex; padding: 1px; border: 1px solid rgb(231 237 244 / 0.06); border-radius: var(--radius); }
.view { flex: 1; min-width: 0; }
</style>
