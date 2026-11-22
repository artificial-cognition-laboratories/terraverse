<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, provide, ref, shallowRef, watch } from "vue"
import type { CreativeItem, SlotRef, Stack } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { SearchBox } from "../../kit/index.ts"
import ItemSlot from "../ItemSlot.vue"
import "../cell.css"
import { HOLE } from "../figure/hole.ts"
import { formatMass } from "../mass.ts"
import { useStore } from "../store.ts"
import { Carry } from "./carry.ts"
import CreativePanel from "./CreativePanel.vue"
import SlotGrid from "./SlotGrid.vue"
import SurvivalPanel from "./SurvivalPanel.vue"

/**
 * The inventory, laid out as Minecraft's: in survival, the player's own
 * inventory; in creative, the catalogue in tabs down its side, the search box
 * always there and always holding the keyboard, so typing searches. One frame
 * whose size never changes with the tab. Esc closes it, and so does E where
 * there's no search to type an E into. No header: the frame is the
 * hotbar's strip grown, and the hotbar itself stays where it always is,
 * drawn again over the HUD's so its slots take what's carried. What's carried, and every click, is in
 * carry.ts; the engine owns every stack.
 */
const bridge = useBridge()
const store = useStore()
const carry = Carry({ bridge: bridge, store: store })

const creative = computed(() => store.ui.creative === true)
const catalog = shallowRef<CreativeItem[] | null>(null)
const tab = ref("building")
const query = ref("")
const search = ref<InstanceType<typeof SearchBox> | null>(null)
const searching = computed(() => creative.value && tab.value !== "inventory")
const row = computed(() => store.state.hotbar?.size ?? 9)
// where the figure's drawn: nothing of this screen's glass may be over it (the engine draws it under the page)
const hole = ref<DOMRect | null>(null)
provide(HOLE, hole)
const layer = ref<HTMLElement | null>(null)
// (against the layer's own corner, which a mask measures from: the window's, but for the gallery's stage)
const holeStyle = computed(() => {
    const at = layer.value?.getBoundingClientRect()
    return hole.value && at && {
        "--hole-x": `${hole.value.left - at.left}px`, "--hole-y": `${hole.value.top - at.top}px`,
        "--hole-w": `${hole.value.width}px`, "--hole-h": `${hole.value.height}px`,
    }
})
const over = ref<{ slot: SlotRef; stack: Stack | null } | { item: CreativeItem } | { part: string } | { note: string } | null>(null)

// the catalogue's tabs, each drawn as an item from it (as Minecraft's)
const TABS = [
    { id: "building", label: "Building blocks", icon: "core:stone_bricks" },
    { id: "nature", label: "Nature", icon: "core:grass" },
    { id: "matter", label: "Matter: every substance", icon: "core:copper" },
    { id: "items", label: "Items", icon: "" },
]
const tabs = computed(() => TABS.flatMap((t) => {
    const inTab = (catalog.value ?? []).filter((i) => i.category === t.id)
    if (!inTab.length) return []
    const face = inTab.find((i) => i.item === t.icon) ?? inTab[0]!
    return [{ ...t, face: face }]
}))

watch(creative, async (on) => {
    if (!on || catalog.value) return
    try {
        catalog.value = await bridge.send("creative.list", {})
        for (const item of catalog.value) store.items[item.item] = { description: item.description, icon: item.icon, mass: item.mass, faces: item.faces }
    } catch (err) {
        carry.problem.value = err instanceof Error ? err.message : String(err)
    }
}, { immediate: true })

function choose(id: string) {
    tab.value = id
    query.value = ""
}

function close() {
    carry.drop()
    bridge.send("screen.close", {})
}

function keydown(e: KeyboardEvent) {
    if (e.key === "Escape") { e.preventDefault(); close(); return }
    // a digit over a slot puts that item on the hotbar; anywhere else in a search it's typed
    if (/^[1-9]$/.test(e.key) && (!searching.value || over.value)) { e.preventDefault(); carry.key(Number(e.key), over.value && ("part" in over.value || "note" in over.value) ? null : over.value); return }
    if (searching.value) return
    if (e.key === "e" || e.key === "E") { e.preventDefault(); close() }
}
// the engine only sends the page keys while an input has focus: nothing here may take it from the search
function keepFocus(e: MouseEvent) {
    if (!(e.target instanceof HTMLInputElement)) e.preventDefault()
}
watch(searching, () => nextTick(() => search.value?.focus()))
function move(e: MouseEvent) { carry.cursor.value = { x: e.clientX, y: e.clientY } }
function up(e: MouseEvent) { carry.release(e, document.elementFromPoint(e.clientX, e.clientY)) }

// craftresult is where a craft lands before the hand places it; it isn't shown,
// so anything left there goes back to the inventory rather than vanish
function returnCraftResult() {
    const stack = store.state.inventory?.craftresult?.stacks[0]
    if (stack) bridge.send("inventory.move", { from: { list: "craftresult", index: 0 }, to: { list: "main" }, count: stack.count }).catch(console.error)
}

onMounted(() => {
    window.addEventListener("keydown", keydown, true) // capture: before an input or button can keep it
    window.addEventListener("mousemove", move)
    window.addEventListener("mouseup", up)
    returnCraftResult()
    search.value?.focus()
})
onBeforeUnmount(() => {
    search.value?.blur() // or the engine's keys stay with a page that's gone
    window.removeEventListener("keydown", keydown, true)
    window.removeEventListener("mousemove", move)
    window.removeEventListener("mouseup", up)
    returnCraftResult()
})

const tooltip = computed(() => {
    if (carry.busy.value || !over.value) return null
    if ("item" in over.value) return over.value.item.description
    if ("part" in over.value) return `${over.value.part} · click to carry it`
    if ("note" in over.value) return over.value.note
    const s = over.value.stack
    if (!s) return null
    const info = store.items[s.item]
    const name = info?.description ?? s.item
    return info?.mass != null ? `${name} · ${s.count} × ${formatMass(info.mass, true)}` : name
})
</script>

<template>
    <div class="screen" @mousedown="keepFocus" @contextmenu.prevent>
        <!-- the backdrop and the frame, cut round the figure (HOLE): what's carried, the tooltip and the hotbar
             are drawn over everything, the figure too -->
        <div ref="layer" class="layer" :class="{ holed: holeStyle }" :style="holeStyle" @mousedown.self="carry.away()">
            <div class="frame" data-panel>
                <!-- the hotbar's strip stood on end: the chosen tab is lit as the held slot is -->
                <nav v-if="creative" class="tabs item-strip" aria-label="Categories">
                    <button v-for="t in tabs" :key="t.id" class="item-cell pickable" :class="{ selected: tab === t.id && !query }"
                        :aria-label="t.label" @click="choose(t.id)" @mouseenter="over = { note: t.label }" @mouseleave="over = null">
                        <img v-if="t.face.icon" :src="t.face.icon" alt="" draggable="false">
                    </button>
                    <button class="item-cell pickable" :class="{ selected: tab === 'parts' && !query }" aria-label="Parts"
                        @click="choose('parts')" @mouseenter="over = { note: 'Parts: wheels, sprung corners, yours' }" @mouseleave="over = null">
                        <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                            <path fill="currentColor" d="M3 0h3v1h-3zM1 1h2v1h-2zM6 1h2v1h-2zM1 2h1v1h-1zM7 2h1v1h-1zM0 3h1v3h-1zM8 3h1v3h-1zM3 3h3v3h-3zM1 6h1v1h-1zM7 6h1v1h-1zM1 7h2v1h-2zM6 7h2v1h-2zM3 8h3v1h-3z" />
                        </svg>
                    </button>
                    <button class="item-cell pickable" :class="{ selected: tab === 'inventory' }" aria-label="Your inventory"
                        @click="choose('inventory')" @mouseenter="over = { note: 'Your inventory' }" @mouseleave="over = null">
                        <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                            <path fill="currentColor" d="M0 1h9v1h-9zM0 2h1v6h-1zM8 2h1v6h-1zM0 8h9v1h-9zM1 4h7v1h-7zM4 3h1v3h-1z" />
                        </svg>
                    </button>
                </nav>

                <div class="content" :class="{ creative: creative }">
                        <div v-if="searching" class="find">
                            <SearchBox ref="search" v-model="query" label="Search everything" />
                            <button class="bin item-cell pickable" data-bin title="Destroy what you carry" @mousedown.prevent="carry.bin()">
                                <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                                    <path fill="currentColor" d="M1 1h7v1h-7zM3 0h3v1h-3zM2 2h5v7h-5zM3 3v5h1v-5zM5 3v5h1v-5z" />
                                </svg>
                            </button>
                        </div>
                        <CreativePanel v-if="searching" :carry="carry" :items="catalog ?? []" :category="tab" :query="query"
                            @hover="over = $event" @taken="close" />
                        <SurvivalPanel v-else :carry="carry" @hover="over = $event" />
                        <p v-if="carry.problem.value" class="problem">{{ carry.problem.value }}</p>
                </div>
            </div>

        </div>

        <!-- the HUD's hotbar, exactly over it (Hud: .bottom), its slots this screen's to drop on -->
        <div v-if="carry.lists.value?.main" class="hotbar item-strip">
            <SlotGrid list="main" :stacks="carry.lists.value.main.stacks" :columns="row" :start="0" :end="row"
                :held="carry.held.value" :selected="store.state.hotbar?.selected" @press="carry.slot" @hover="over = $event" />
        </div>

        <div v-if="carry.carried.value" class="hand" :style="{ left: `${carry.cursor.value.x}px`, top: `${carry.cursor.value.y}px` }">
            <ItemSlot :stack="carry.carried.value" />
        </div>
        <div v-else-if="tooltip" class="tooltip" :style="{ left: `${carry.cursor.value.x + 16}px`, top: `${carry.cursor.value.y + 16}px` }">
            {{ tooltip }}
        </div>
    </div>
</template>

<style scoped>
.screen { --u: 0.125rem; /* the HUD's pixel (Hud): items here are the hotbar's size */ position: fixed; inset: 0; }
.layer {
    position: absolute;
    inset: 0;
    display: grid;
    place-items: center;
    /* between 20% and 40% solid the shell frosts the world only partly (theme.css, --glass): a light blur */
    background: rgb(4 7 11 / 0.3);
}
/* everything, less the hole: the window's whole box, its hole's taken away */
.layer.holed {
    -webkit-mask: linear-gradient(#000 0 0), linear-gradient(#000 0 0) var(--hole-x) var(--hole-y) / var(--hole-w) var(--hole-h) no-repeat;
    -webkit-mask-composite: xor;
    mask-composite: exclude;
}

/* nine cells across, in the hotbar's strip (cell.css) grown to hold them, survival or creative; in
   creative one height whatever the tab: the search and six rows down (the scrollbar is a hairline
   in the padding, CreativePanel, so it takes no width) */
.frame { position: relative; display: flex; align-items: flex-start; }
.content {
    width: calc(2.75rem * 9 + 0.25rem * 10 + 2px);
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    padding: 0.25rem;
    border: 1px solid var(--glass-edge);
    background: rgb(4 7 11 / 0.72);
}
.content.creative { min-height: calc(2.75rem * 7 + 0.25rem * 8 + 2px); }
.find { display: flex; gap: 0.25rem; }
/* the site's search made a long cell (cell.css): the slots' hairline and lift, no glass of its own,
   its edge lit while it holds the keyboard as a hovered cell's is */
.find :deep(.search-box) { flex: 1; min-width: 0; height: 2.75rem; border-color: rgb(231 237 244 / 0.06);
    background: rgb(231 237 244 / 0.025); backdrop-filter: none; }
.find :deep(.search-box:focus-within) { border-color: var(--edge-lit); }
.bin.item-cell:hover { color: var(--danger); border-color: rgb(248 81 73 / 0.5); }
.bin svg { width: calc(var(--u) * 9); height: calc(var(--u) * 9); }

/* where the HUD draws it (Hud: .bottom, 1rem up, centred) */
.hotbar { position: fixed; left: 50%; bottom: 1rem; transform: translateX(-50%); }

/* beside the frame, not in it: its own strip, its labels the screen's tooltip */
.tabs { position: absolute; right: calc(100% + 0.25rem); top: 0; flex-direction: column; }
.tabs img { width: calc(var(--u) * 16); height: calc(var(--u) * 16); image-rendering: pixelated; }
.tabs svg { width: calc(var(--u) * 9); height: calc(var(--u) * 9); }

.problem { margin: 0; padding: 0.25rem; color: var(--danger); font: 0.75rem var(--font-sans); }

.hand { position: fixed; pointer-events: none; transform: translate(-50%, -50%); filter: drop-shadow(2px 2px 0 rgb(0 0 0 / 0.5)); }
.tooltip { position: fixed; pointer-events: none; padding: 0.4rem 0.7rem; border: 1px solid var(--glass-edge); background: var(--glass-strong);
    color: var(--ink); font: 0.8rem var(--font-sans); white-space: pre-line; }
</style>
