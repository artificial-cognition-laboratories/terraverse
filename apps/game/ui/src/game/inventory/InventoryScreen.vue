<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import type { CreativeItem, SlotRef, Stack } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Button, IconButton, Panel, SearchBox } from "../../kit/index.ts"
import ItemSlot from "../ItemSlot.vue"
import { useStore } from "../store.ts"
import { Carry } from "./carry.ts"
import CreativePanel from "./CreativePanel.vue"
import SurvivalPanel from "./SurvivalPanel.vue"

/**
 * The inventory, laid out as Minecraft's: in survival, the player's own
 * inventory; in creative, the catalogue in tabs down its side, the search box
 * always there and always holding the keyboard, so typing searches. One frame
 * whose size never changes with the tab. Esc closes it, and so does I where
 * there's no search to type an I into. What's carried, and every click, is in
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
const over = ref<{ slot: SlotRef; stack: Stack | null } | { item: CreativeItem } | { part: string } | null>(null)

// the catalogue's tabs, each drawn as an item from it (as Minecraft's)
const TABS = [
    { id: "building", label: "Building blocks", icon: "tf_world:stone_bricks" },
    { id: "nature", label: "Nature", icon: "tf_world:grass" },
    { id: "matter", label: "Matter: every substance", icon: "tf_matter:copper" },
    { id: "functional", label: "Tools for finding out", icon: "tf_matter:heat_gun" },
    { id: "tools", label: "Tools", icon: "tf_matter:fire_drill" },
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
        for (const item of catalog.value) store.items[item.item] = { description: item.description, icon: item.icon }
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
    if (/^[1-9]$/.test(e.key) && (!searching.value || over.value)) { e.preventDefault(); carry.key(Number(e.key), over.value && "part" in over.value ? null : over.value); return }
    if (searching.value) return
    if (e.key === "i" || e.key === "I") { e.preventDefault(); close() }
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
    const s = over.value.stack
    return s ? store.items[s.item]?.description ?? s.item : null
})
</script>

<template>
    <div class="screen" @mousedown="keepFocus" @mousedown.self="carry.away()" @contextmenu.prevent>
        <div class="frame" data-panel>
            <nav v-if="creative" class="tabs" aria-label="Categories">
                <IconButton v-for="t in tabs" :key="t.id" :label="t.label" side="left" :on="tab === t.id && !query" @click="choose(t.id)">
                    <img v-if="t.face.icon" :src="t.face.icon" alt="" draggable="false">
                </IconButton>
                <IconButton label="Parts: wheels, sprung corners, yours" side="left" :on="tab === 'parts' && !query" @click="choose('parts')">
                    <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M3 0h3v1h-3zM1 1h2v1h-2zM6 1h2v1h-2zM1 2h1v1h-1zM7 2h1v1h-1zM0 3h1v3h-1zM8 3h1v3h-1zM3 3h3v3h-3zM1 6h1v1h-1zM7 6h1v1h-1zM1 7h2v1h-2zM6 7h2v1h-2zM3 8h3v1h-3z" />
                    </svg>
                </IconButton>
                <IconButton label="Your inventory" side="left" :on="tab === 'inventory'" @click="choose('inventory')">
                    <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M0 1h9v1h-9zM0 2h1v6h-1zM8 2h1v6h-1zM0 8h9v1h-9zM1 4h7v1h-7zM4 3h1v3h-1z" />
                    </svg>
                </IconButton>
            </nav>

            <Panel :title="!creative || tab === 'inventory' ? 'Inventory' : query ? 'Search' : tab === 'parts' ? 'Parts' : tabs.find((t) => t.id === tab)?.label ?? ''">
                <template #actions>
                    <Button size="sm" @click="close">Close</Button>
                </template>
                <div class="content">
                    <SearchBox v-if="searching" ref="search" v-model="query" label="Search everything" />
                    <CreativePanel v-if="searching" :carry="carry" :items="catalog ?? []" :category="tab" :query="query"
                        @hover="over = $event" @taken="close" />
                    <SurvivalPanel v-else :carry="carry" @hover="over = $event" />
                    <p v-if="carry.problem.value" class="problem">{{ carry.problem.value }}</p>
                </div>
            </Panel>
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
.screen {
    --u: clamp(2px, round(down, 100vh / 300, 1px), 3px);
    position: fixed;
    inset: 0;
    display: grid;
    place-items: center;
    background: rgb(4 8 12 / 0.45);
}

/* one size, whatever the tab: the hotbar and its bin across, the search, six rows and the hotbar down */
.frame { position: relative; display: flex; align-items: flex-start; }
.content {
    width: calc(var(--u) * (18 * 9 + 8 + 3 + 18));
    min-height: calc(var(--u) * (18 * 7 + 5 + 6) + 32px);
    display: flex;
    flex-direction: column;
    gap: calc(var(--u) * 3);
}

/* beside the panel, not in it: their labels stick out to the left */
.tabs { position: absolute; right: calc(100% + 8px); top: 0; display: flex; flex-direction: column; gap: 6px; }
.tabs img { width: 32px; height: 32px; image-rendering: pixelated; }
.tabs svg { width: 18px; height: 18px; }

.problem { margin: 0; color: var(--danger); font-size: 11px; }

.hand { position: fixed; pointer-events: none; transform: translate(-50%, -50%); filter: drop-shadow(var(--b) var(--b) 0 rgb(0 0 0 / 0.5)); }
.tooltip { position: fixed; pointer-events: none; padding: 6px 10px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.96);
    box-shadow: var(--raised); font: 12px var(--font-display); white-space: pre-line; }
</style>
