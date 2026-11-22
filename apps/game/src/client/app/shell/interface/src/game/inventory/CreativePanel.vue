<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { CreativeItem, SlotRef, Stack } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { useStore } from "../store.ts"
import type { CarryHandle } from "./carry.ts"
import "../cell.css"

/**
 * The creative catalogue: every item by category, searchable, with the
 * search and the bin above, the screen's (InventoryScreen). Typing searches everything. Parts (sprint 77) are in it
 * as items are, in their own tab: the starter parts and the player's
 * library, every world's. One clicked is carried till you click, onto a
 * chassis by the face it hung by (a corner on its spring).
 */
const props = defineProps<{ carry: CarryHandle; items: CreativeItem[]; category: string; query: string }>()
const emit = defineEmits<{
    hover: [over: { slot: SlotRef; stack: Stack | null } | { item: CreativeItem } | { part: string } | null]
    taken: []
}>()
const store = useStore()
const bridge = useBridge()

const shown = computed(() => {
    const q = props.query.trim().toLowerCase()
    return props.items.filter((item) => (q
        ? item.description.toLowerCase().includes(q) || item.item.toLowerCase().includes(q)
        : item.category === props.category))
})

// the parts shown: their tab's, or those a search finds
const parts = computed(() => {
    const q = props.query.trim().toLowerCase()
    const all = store.state.library ?? []
    return q ? all.filter((p) => p.toLowerCase().includes(q)) : props.category === "parts" ? all : []
})
// TEMP: 2026-10-31: a glyph by what it is, not its own picture: a part's
// icon is to be rendered from its mesh, as blocks' are (shell/items)
const wheeled = (part: string) => /wheel|corner/i.test(part)

function take(part: string) {
    emit("taken")
    void bridge.send("tools.ask", { action: "take", name: part })
}

// the scrollbar: a hairline in the frame's padding beside the catalogue, not the browser's, which
// would take a gutter's width; lit while the catalogue moves
const catalogue = ref<HTMLElement | null>(null)
const thumb = ref<{ top: number; height: number } | null>(null)
const moving = ref(false)
let still: ReturnType<typeof setTimeout> | undefined
function measure() {
    const el = catalogue.value
    if (!el || el.scrollHeight <= el.clientHeight) { thumb.value = null; return }
    thumb.value = { top: el.scrollTop / el.scrollHeight * 100, height: el.clientHeight / el.scrollHeight * 100 }
}
function scrolled() {
    measure()
    moving.value = true
    clearTimeout(still)
    still = setTimeout(() => { moving.value = false }, 600)
}
// another tab or search: back to the top
watch([shown, parts], () => nextTick(() => {
    if (catalogue.value) catalogue.value.scrollTop = 0
    measure()
}))
onMounted(measure)
onBeforeUnmount(() => clearTimeout(still))

function down(item: CreativeItem, e: MouseEvent) {
    if (e.button !== 0 && e.button !== 2) return
    props.carry.catalogue(item, e.button === 2 ? "right" : "left", e.shiftKey, e)
}
</script>

<template>
    <div class="creative">
        <div ref="catalogue" class="catalogue" @scroll="scrolled" @mousedown.self="carry.held.value && carry.bin()">
            <button v-for="item in shown" :key="item.item" class="item-cell pickable" :aria-label="item.description"
                @mousedown.prevent="down(item, $event)" @contextmenu.prevent
                @mouseenter="emit('hover', { item: item })" @mouseleave="emit('hover', null)">
                <img v-if="item.icon" :src="item.icon" :alt="item.description" draggable="false">
                <span v-else>{{ item.description.slice(0, 2) }}</span>
            </button>
            <button v-for="p in parts" :key="`part:${p}`" class="item-cell pickable part" :aria-label="p"
                @mousedown.prevent="$event.button === 0 && take(p)" @contextmenu.prevent
                @mouseenter="emit('hover', { part: p })" @mouseleave="emit('hover', null)">
                <svg v-if="wheeled(p)" viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                    <path fill="currentColor" d="M3 0h3v1h-3zM1 1h2v1h-2zM6 1h2v1h-2zM1 2h1v1h-1zM7 2h1v1h-1zM0 3h1v3h-1zM8 3h1v3h-1zM3 3h3v3h-3zM1 6h1v1h-1zM7 6h1v1h-1zM1 7h2v1h-2zM6 7h2v1h-2zM3 8h3v1h-3z" />
                </svg>
                <svg v-else viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                    <path fill="currentColor" d="M0 3h6v3h-6zM6 1h3v7h-3z" />
                </svg>
            </button>
            <p v-if="!shown.length && !parts.length" class="empty">{{ query ? "Nothing matches." : "Nothing here yet." }}</p>
        </div>
        <i v-if="thumb" class="thumb" :class="{ lit: moving }" :style="{ top: `${thumb.top}%`, height: `${thumb.height}%` }" aria-hidden="true" />
    </div>
</template>

<style scoped>
.creative { position: relative; display: flex; flex-direction: column; height: 100%; }
/* nine across, six down, scrolling: the same size whatever the tab */
.catalogue { display: grid; grid-template-columns: repeat(9, 2.75rem); grid-auto-rows: 2.75rem; gap: 0.25rem;
    height: calc(2.75rem * 6 + 0.25rem * 5); overflow-y: auto; overflow-x: hidden; align-content: start; }
.catalogue::-webkit-scrollbar { display: none; }
/* centred in the frame's right padding (InventoryScreen: .content, 0.25rem) */
.thumb { position: absolute; right: calc(-0.125rem - 1px); width: 2px; background: var(--glass-edge-hi); pointer-events: none;
    transition: background 120ms; }
.thumb.lit { background: var(--edge-lit-hi); }
.item-cell img { width: calc(var(--u) * 16); height: calc(var(--u) * 16); image-rendering: pixelated; }
.item-cell > span { font: 0.75rem var(--font-display); color: var(--ink); }
.part svg { width: calc(var(--u) * 12); height: calc(var(--u) * 12); }
.empty { grid-column: 1 / -1; margin: 0; padding: 1rem; color: var(--ink-faint); font: 0.8rem var(--font-sans); }
</style>
