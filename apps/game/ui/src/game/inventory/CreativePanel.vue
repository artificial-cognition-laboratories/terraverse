<script setup lang="ts">
import { computed } from "vue"
import type { CreativeItem, SlotRef, Stack } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { useStore } from "../store.ts"
import type { CarryHandle } from "./carry.ts"
import SlotGrid from "./SlotGrid.vue"

/**
 * The creative catalogue: every item by category, searchable, with the
 * hotbar below to drag into and a bin to throw things away. The search box
 * is always there: typing searches everything. Parts (sprint 77) are in it
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
const row = computed(() => store.state.hotbar?.size ?? 9)
const main = computed(() => props.carry.lists.value?.main ?? null)

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

function down(item: CreativeItem, e: MouseEvent) {
    if (e.button !== 0 && e.button !== 2) return
    props.carry.catalogue(item, e.button === 2 ? "right" : "left", e.shiftKey, e)
}
</script>

<template>
    <div class="creative">
        <div class="catalogue" @mousedown.self="carry.held.value && carry.bin()">
            <button v-for="item in shown" :key="item.item" class="cell" :aria-label="item.description"
                @mousedown.prevent="down(item, $event)" @contextmenu.prevent
                @mouseenter="emit('hover', { item: item })" @mouseleave="emit('hover', null)">
                <img v-if="item.icon" :src="item.icon" :alt="item.description" draggable="false">
                <span v-else>{{ item.description.slice(0, 2) }}</span>
            </button>
            <button v-for="p in parts" :key="`part:${p}`" class="cell part" :aria-label="p"
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
        <div v-if="main" class="bottom">
            <SlotGrid list="main" :stacks="main.stacks" :columns="row" :start="0" :end="row" :held="carry.held.value"
                @press="carry.slot" @hover="emit('hover', $event)" />
            <button class="bin" data-bin title="Destroy what you carry" @mousedown.prevent="carry.bin()">
                <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                    <path fill="currentColor" d="M1 1h7v1h-7zM3 0h3v1h-3zM2 2h5v7h-5zM3 3v5h1v-5zM5 3v5h1v-5z" />
                </svg>
            </button>
        </div>
    </div>
</template>

<style scoped>
.creative { display: flex; flex-direction: column; gap: calc(var(--u) * 3); height: 100%; }
/* nine across, six down, scrolling, with the scrollbar's room kept in the frame's width: the same size whatever the tab */
.catalogue { display: grid; grid-template-columns: repeat(9, calc(var(--u) * 18)); grid-auto-rows: calc(var(--u) * 18); gap: var(--u);
    height: calc(var(--u) * 18 * 6 + var(--u) * 5); overflow-y: auto; overflow-x: hidden; align-content: start; scrollbar-gutter: stable; }
.cell { display: grid; place-items: center; padding: 0; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.cell:hover { background: #1b2730; box-shadow: inset 0 0 0 var(--b) var(--accent-lo), var(--sunk); }
.cell img { width: calc(var(--u) * 16); height: calc(var(--u) * 16); image-rendering: pixelated; }
.cell span { font: 12px var(--font-display); }
.cell.part { color: var(--ink-dim); }
.cell.part svg { width: calc(var(--u) * 12); height: calc(var(--u) * 12); }
.empty { grid-column: 1 / -1; color: var(--ink-faint); padding: 16px; margin: 0; }
.bottom { display: flex; align-items: center; gap: calc(var(--u) * 3); }
.bin { width: calc(var(--u) * 18); height: calc(var(--u) * 18); display: grid; place-items: center; padding: 0;
    border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--raised); color: var(--ink-dim); }
.bin:hover { color: var(--danger); }
.bin svg { width: calc(var(--u) * 9); height: calc(var(--u) * 9); }
</style>
