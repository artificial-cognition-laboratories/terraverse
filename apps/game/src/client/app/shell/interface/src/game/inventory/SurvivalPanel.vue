<script setup lang="ts">
import { computed } from "vue"
import type { SlotRef, Stack } from "../../bridge/index.ts"
import { useStore } from "../store.ts"
import type { CarryHandle } from "./carry.ts"
import FigureWell from "../figure/FigureWell.vue"
import Offers from "../making/Offers.vue"
import SlotGrid from "./SlotGrid.vue"

/**
 * The player's own inventory: what they wear beside their figure (drawn by the engine: FigureWell), the 2×2 crafting grid and
 * what its things could become worked by what's on the cursor (making/Offers.vue), and storage (the hotbar is the HUD's: InventoryScreen).
 */
const props = defineProps<{ carry: CarryHandle }>()
const emit = defineEmits<{ hover: [over: { slot: SlotRef; stack: Stack | null } | { note: string } | null] }>()
const store = useStore()
const lists = computed(() => props.carry.lists.value)
const row = computed(() => store.state.hotbar?.size ?? 9)
// TEMP: 2026-11-22: what's worn has no list in the engine yet (server/players/player.cpp gives
// main and craft only): the four slots are drawn, empty, and say so rather than take anything
const BODY = [
    { id: "head", label: "Head", path: "M3 1h3v1h1v3h-1v1h-3v-1h-1v-3h1zM2 7h5v1h-5z" },
    { id: "body", label: "Body", path: "M2 1h5v1h2v3h-2v4h-5v-4h-2v-3h2z" },
    { id: "legs", label: "Legs", path: "M2 0h5v9h-2v-6h-1v6h-2z" },
    { id: "feet", label: "Feet", path: "M1 4h2v3h1v2h-3zM5 4h2v5h-2v-2h1z" },
]
const unworn = (label: string) => `${label} · wearing things isn't built yet`
</script>

<template>
    <div v-if="lists?.main && lists.craft" class="survival">
        <section class="top">
            <div class="body">
                <div v-for="b in BODY" :key="b.id" class="item-cell worn" :aria-label="unworn(b.label)"
                    @mouseenter="emit('hover', { note: unworn(b.label) })" @mouseleave="emit('hover', null)">
                    <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true"><path fill="currentColor" :d="b.path" /></svg>
                </div>
            </div>
            <FigureWell class="figure" />
            <div class="crafting">
                <div class="craft-row">
                    <SlotGrid list="craft" :stacks="lists.craft.stacks" :columns="lists.craft.width" :held="carry.held.value"
                        @press="carry.slot" @hover="emit('hover', $event)" />
                    <svg class="arrow" viewBox="0 0 11 7" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M0 3h7v-3h1v1h1v1h1v1h1v1h-1v1h-1v1h-1v1h-1v-3h-7z" />
                    </svg>
                    <Offers :grid="lists.craft.stacks" :carried="carry.carried.value" @hover="emit('hover', $event)"
                        @pick="carry.problem.value = 'Making isn\'t wired to the engine yet: nothing was made.'" />
                </div>
            </div>
        </section>
        <SlotGrid list="main" :stacks="lists.main.stacks" :columns="row" :start="row" :held="carry.held.value"
            @press="carry.slot" @hover="emit('hover', $event)" />
    </div>
</template>

<style scoped>
/* as tight as the hotbar: a cell's gap between everything. Along the top, four cells tall: what's worn
   in a column, the figure's well beside it, then the crafting */
.survival { display: flex; flex-direction: column; gap: 0.25rem; }
.top { display: flex; gap: 0.25rem; height: calc(2.75rem * 4 + 0.25rem * 3); }
.body { display: flex; flex-direction: column; gap: 0.25rem; }
.worn svg { width: calc(var(--u) * 11); height: calc(var(--u) * 11); color: rgb(231 237 244 / 0.12); }
.figure { flex: none; width: calc(2.75rem * 3 + 0.25rem * 2); }
.crafting { display: flex; flex: 1; align-items: center; justify-content: center; border: 1px solid rgb(231 237 244 / 0.06); border-radius: var(--radius); }
.craft-row { display: flex; align-items: center; gap: 1rem; }
.arrow { width: 1.4rem; height: 0.9rem; color: var(--ink-faint); }
</style>
