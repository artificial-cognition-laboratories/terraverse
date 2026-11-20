<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { useStore } from "../game/store.ts"

// The body under the mouse on the Planetarium (the engine says which, and
// where it's drawn): four corners closing round its disc, its name and kind
// beside them, a heads-up readout. Never takes the mouse.
const store = useStore()
// The page takes the mouse over its own pixels (src/shell/input.cpp), so the
// engine doesn't hear it leave the planet for a panel: while the pointer is on
// the page, the card's hidden. (Off the page, the page hears it leave.)
const onPage = ref(false)
const over = () => { onPage.value = true }
const out = (e: PointerEvent) => { if (!e.relatedTarget) onPage.value = false }
document.addEventListener("pointerover", over)
document.addEventListener("pointerout", out)
onBeforeUnmount(() => {
    clearTimeout(leaving)
    store.hover = null // the engine only says while it's shown
    document.removeEventListener("pointerover", over)
    document.removeEventListener("pointerout", out)
})
const PAD = 6, LEAST = 14 // page pixels: round the disc; the smallest box's half
// what's shown: the engine's hover, kept a moment after it goes (a body
// passing under the mouse in a flight, a frame's gap) so the card doesn't blink
const LINGER = 180 // ms
const shown = ref(store.hover)
let leaving: ReturnType<typeof setTimeout> | undefined
watch(() => store.hover, (h) => {
    clearTimeout(leaving)
    if (h) shown.value = h
    else leaving = setTimeout(() => { shown.value = null }, LINGER)
})
const box = computed(() => {
    const h = shown.value
    if (!h || onPage.value) return null
    const half = Math.max(LEAST, h.radius + PAD)
    return { left: h.x - half, top: h.y - half, size: half * 2 }
})
</script>

<template>
    <div v-if="box && shown" :key="shown.id" class="hover"
        :style="{ left: `${box.left}px`, top: `${box.top}px`, width: `${box.size}px`, height: `${box.size}px` }">
        <i class="c tl" /><i class="c tr" /><i class="c bl" /><i class="c br" />
        <div class="readout">
            <strong>{{ shown.name }}</strong>
            <span>{{ shown.kind }}</span>
            <em>double-click to focus</em>
        </div>
    </div>
</template>

<style scoped>
.hover { position: fixed; pointer-events: none; z-index: 5; animation: close-in 140ms ease-out; }
.c { position: absolute; width: 9px; height: 9px; border: 0 solid var(--accent-hi); filter: drop-shadow(0 0 3px rgb(0 148 210 / 0.6)); }
.tl { left: 0; top: 0; border-left-width: 2px; border-top-width: 2px; }
.tr { right: 0; top: 0; border-right-width: 2px; border-top-width: 2px; }
.bl { left: 0; bottom: 0; border-left-width: 2px; border-bottom-width: 2px; }
.br { right: 0; bottom: 0; border-right-width: 2px; border-bottom-width: 2px; }
/* beside the box's top right, a short rule leading to it */
.readout { position: absolute; left: calc(100% + 10px); top: -2px; display: grid; gap: 2px; padding: 4px 8px 5px; white-space: nowrap;
    background: rgb(6 12 16 / 0.72); border-left: 2px solid var(--accent-hi); }
.readout::before { content: ""; position: absolute; right: 100%; top: 6px; width: 10px; border-top: 1px solid var(--accent-hi); }
.readout strong { font: 12px var(--font-display); color: var(--ink); letter-spacing: 0.06em; text-transform: uppercase; }
.readout span { font: 11px var(--font-display); color: var(--accent-hi); text-transform: uppercase; }
.readout em { font: 10px var(--font-display); font-style: normal; color: var(--ink-faint); }
@keyframes close-in { from { transform: scale(1.35); opacity: 0; } to { transform: scale(1); opacity: 1; } }
</style>
