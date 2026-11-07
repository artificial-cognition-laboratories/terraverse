<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { MakingOffer, SlotRef, Stack } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Button, ListEmpty, ListGroup, ListItem, Notice, Panel, Progress } from "../../kit/index.ts"
import ItemSlot from "../ItemSlot.vue"
import { Carry, type NodeLists } from "../inventory/carry.ts"
import Slot from "../inventory/Slot.vue"
import SlotGrid from "../inventory/SlotGrid.vue"
import { formatMass } from "../mass.ts"
import { useStore } from "../store.ts"

/**
 * Making (tf_craft), laid out as Minecraft's crafting table: a station's
 * worktop of slots above your own inventory and hotbar, stacks dragged
 * between them as in the inventory (carry.ts). Beside it, the book of what can
 * be made from what's on the worktop and in your pockets, and what can't yet
 * and why, so there's no pattern to learn: pick one, Make, and it's made from
 * the worktop first. The server says what's offered and makes it.
 */
const bridge = useBridge()
const store = useStore()

const making = computed(() => store.ui.making)
// the worktop as the inventory lists are, for carry.ts: false (Lua's empty slot) is null here
const worktop = computed<NodeLists | null>(() => {
    const m = making.value
    if (!m?.at || !m.worktop) return null
    return { worktop: { width: m.worktop.width, stacks: m.worktop.stacks.map((s) => s || null), at: m.at } }
})
const carry = Carry({ bridge: bridge, store: store, nodes: () => worktop.value })
const lists = computed(() => carry.lists.value)
const row = computed(() => store.state.hotbar?.size ?? 9)

// null offers: nothing in reach to work (sent so for an empty list)
const offers = computed(() => making.value?.offers ?? [])
const ready = computed(() => offers.value.filter((o) => o.ok))
const notYet = computed(() => offers.value.filter((o) => !o.ok))
const picked = ref<string | null>(null)
const chosen = computed<MakingOffer | null>(() => offers.value.find((o) => o.id === picked.value) ?? null)
const working = computed(() => making.value?.working ?? null)
const workingOn = computed(() => working.value && offers.value.find((o) => o.id === working.value!.id))
// (an offer for a form the material can't take has no item to show)
const result = computed<Stack | null>(() => chosen.value?.item ? { item: chosen.value.item, count: chosen.value.count, wear: 0 } : null)

// the first that can be made, until the player picks; kept while it's still offered
watch(offers, (now) => {
    if (!now.some((o) => o.id === picked.value)) picked.value = (now.find((o) => o.ok) ?? now[0])?.id ?? null
}, { immediate: true })

function make() {
    if (!chosen.value?.ok || working.value) return
    bridge.send("ui.send", { name: "making.make", payload: { offer: chosen.value.id } }).catch(console.error)
}
function close() {
    carry.drop()
    bridge.send("screen.close", {}).catch(console.error)
}

const over = ref<{ slot: SlotRef; stack: Stack | null } | null>(null)
const tooltip = computed(() => {
    const s = over.value?.stack
    if (carry.busy.value || !s) return null
    const info = store.items[s.item]
    const name = info?.description ?? s.item
    return info?.mass != null ? `${name} · ${s.count} × ${formatMass(info.mass, true)}` : name
})

function keydown(e: KeyboardEvent) {
    if (e.key === "Escape" || e.key === "e" || e.key === "E") { e.preventDefault(); close(); return }
    if (/^[1-9]$/.test(e.key)) { e.preventDefault(); carry.key(Number(e.key), over.value) }
}
function move(e: MouseEvent) { carry.cursor.value = { x: e.clientX, y: e.clientY } }
function up(e: MouseEvent) { carry.release(e, document.elementFromPoint(e.clientX, e.clientY)) }
onMounted(() => {
    window.addEventListener("keydown", keydown, true)
    window.addEventListener("mousemove", move)
    window.addEventListener("mouseup", up)
})
onBeforeUnmount(() => {
    window.removeEventListener("keydown", keydown, true)
    window.removeEventListener("mousemove", move)
    window.removeEventListener("mouseup", up)
})

function icon(item: string | undefined) { return item ? store.items[item]?.icon ?? null : null }
function seconds(s: number) { return `${Math.round(s * 10) / 10} s` }
</script>

<template>
    <div class="screen" @mousedown.self="carry.away()" @contextmenu.prevent>
        <Panel :title="making?.station ?? 'Making by hand'" class="making" data-panel>
            <template #actions><Button size="sm" @click="close">Close</Button></template>
            <div v-if="making" class="columns">
                <div class="book">
                    <ListGroup v-if="ready.length" title="Can make">
                        <ListItem v-for="o in ready" :key="o.id" :on="o.id === picked" @click="picked = o.id" @dblclick="make">
                            <template #lead><img v-if="icon(o.item)" :src="icon(o.item)!" alt=""><span v-else>{{ o.process }}</span></template>
                            {{ o.label }}<template v-if="o.count > 1"> ×{{ o.count }}</template>
                            <template #sub>{{ o.process }} · {{ seconds(o.seconds) }}</template>
                        </ListItem>
                    </ListGroup>
                    <ListGroup v-if="notYet.length" title="Not yet">
                        <ListItem v-for="o in notYet" :key="o.id" faint :on="o.id === picked" @click="picked = o.id">
                            <template #lead><img v-if="icon(o.item)" :src="icon(o.item)!" alt=""><span v-else>{{ o.process }}</span></template>
                            {{ o.label }}<template #sub>{{ o.reason }}</template>
                        </ListItem>
                    </ListGroup>
                    <ListEmpty v-if="!offers.length">Nothing in reach to work.</ListEmpty>
                </div>

                <div v-if="lists?.main" class="bench">
                    <section class="top">
                        <div class="worktop">
                            <span class="label">{{ worktop ? "Worktop" : "In hand" }}</span>
                            <SlotGrid v-if="worktop && lists.worktop" list="worktop" :stacks="lists.worktop.stacks" :columns="lists.worktop.width"
                                :held="carry.held.value" @press="carry.slot" @hover="over = $event" />
                            <p v-else class="dim hand">No worktop: what's in your pockets, slower. A workbench is lashed from sticks and cord.</p>
                        </div>
                        <svg class="arrow" viewBox="0 0 11 7" shape-rendering="crispEdges" aria-hidden="true">
                            <path fill="currentColor" d="M0 3h7v-3h1v1h1v1h1v1h1v1h-1v1h-1v1h-1v1h-1v-3h-7z" />
                        </svg>
                        <div class="out">
                            <div class="result" :class="{ faint: chosen && !chosen.ok }"><ItemSlot :stack="result" /></div>
                            <template v-if="chosen">
                                <span class="what">{{ chosen.label }}</span>
                                <span class="dim">{{ chosen.process }}{{ chosen.tool ? ` with ${chosen.tool.toLowerCase()}` : "" }} · {{ seconds(chosen.seconds) }}</span>
                            </template>
                        </div>
                    </section>

                    <dl v-if="chosen" class="inputs">
                        <div v-for="input in chosen.inputs" :key="input.label" :class="{ short: input.have < input.count }">
                            <dt>{{ input.label }}</dt><dd>{{ input.have }} / {{ input.count }}</dd>
                        </div>
                    </dl>
                    <Notice v-if="chosen && !chosen.ok" tone="warn">{{ chosen.reason }}</Notice>
                    <Notice v-else-if="!chosen">Pick up sticks, stones and plant fibre: leaves drop sticks, plants fibre, gravel stones and flint.</Notice>
                    <div class="act">
                        <Progress v-if="working" :key="working.id + working.seconds" :seconds="working.seconds"
                            :label="workingOn ? `${workingOn.process}: ${workingOn.label}` : 'Working'" />
                        <Button v-else variant="primary" :disabled="!chosen?.ok" @click="make">Make »</Button>
                    </div>

                    <SlotGrid list="main" :stacks="lists.main.stacks" :columns="row" :start="row" :held="carry.held.value"
                        @press="carry.slot" @hover="over = $event" />
                    <SlotGrid list="main" :stacks="lists.main.stacks" :columns="row" :start="0" :end="row" :held="carry.held.value"
                        class="hotbar" @press="carry.slot" @hover="over = $event" />
                    <p v-if="carry.problem.value" class="problem">{{ carry.problem.value }}</p>
                </div>
            </div>
        </Panel>

        <div v-if="carry.carried.value" class="hand-held" :style="{ left: `${carry.cursor.value.x}px`, top: `${carry.cursor.value.y}px` }">
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
    position: fixed; inset: 0; display: grid; place-items: center; background: rgb(4 8 12 / 0.45);
}
.making { width: min(calc(var(--u) * (18 * 9 + 8) + 300px), calc(100vw - 32px)); }
.columns { display: grid; grid-template-columns: minmax(0, 240px) minmax(0, 1fr); gap: 18px; }
.book { min-width: 0; max-height: calc(var(--u) * 150); overflow: hidden auto; }
.book img { width: 32px; height: 32px; image-rendering: pixelated; }
.bench { min-width: 0; display: flex; flex-direction: column; gap: calc(var(--u) * 3); }
.top { display: flex; align-items: center; gap: calc(var(--u) * 6); }
.worktop { display: flex; flex-direction: column; gap: calc(var(--u) * 2); }
.label { font: 11px var(--font-display); text-transform: uppercase; color: var(--ink-dim); }
.hand { width: calc(var(--u) * 56); margin: 0; }
.arrow { width: calc(var(--u) * 11); height: calc(var(--u) * 7); color: var(--stone-4); flex: none; }
.out { min-width: 0; display: flex; flex-direction: column; gap: 4px; }
.result { display: grid; place-items: center; width: calc(var(--u) * 24); height: calc(var(--u) * 24); border: var(--b) solid var(--edge);
    background: var(--stone-0); box-shadow: var(--sunk); }
.result.faint { opacity: 0.45; }
.what { font: 400 13px var(--font-display); text-transform: uppercase; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.dim { color: var(--ink-dim); font-size: 12px; }
.inputs { margin: 0; display: flex; flex-direction: column; gap: 4px; }
.inputs div { display: flex; justify-content: space-between; font-size: 12px; padding: 4px 8px; background: var(--stone-2); box-shadow: var(--sunk); }
.inputs div.short dd { color: var(--danger); }
dt { color: var(--ink-dim); }
dd { margin: 0; }
.act { display: flex; flex-direction: column; }
.hotbar { margin-top: calc(var(--u) * 2); }
.problem { margin: 0; color: var(--danger); font-size: 12px; }
.hand-held { position: fixed; pointer-events: none; transform: translate(-50%, -50%); width: calc(var(--u) * 16); height: calc(var(--u) * 16); }
.tooltip { position: fixed; pointer-events: none; padding: 6px 10px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.96);
    font-size: 12px; white-space: nowrap; }
</style>
