<script setup lang="ts">
import { computed } from "vue"
import type { SlotRef, Stack } from "../../bridge/index.ts"
import { useStore } from "../store.ts"
import type { CarryHandle } from "./carry.ts"
import Slot from "./Slot.vue"
import SlotGrid from "./SlotGrid.vue"

/** The player's own inventory: the 2×2 crafting grid and its result, storage, and the hotbar. */
const props = defineProps<{ carry: CarryHandle }>()
const emit = defineEmits<{ hover: [over: { slot: SlotRef; stack: Stack | null } | null] }>()
const store = useStore()
const lists = computed(() => props.carry.lists.value)
const row = computed(() => store.state.hotbar?.size ?? 9)
const preview = computed(() => lists.value?.craftpreview?.stacks[0] ?? null)
</script>

<template>
    <div v-if="lists?.main && lists.craft" class="survival">
        <section class="top">
            <div class="figure" aria-hidden="true"><span>the player model comes here</span></div>
            <div class="crafting">
                <span class="label">Crafting</span>
                <div class="craft-row">
                    <SlotGrid list="craft" :stacks="lists.craft.stacks" :columns="lists.craft.width" :held="carry.held.value"
                        @press="carry.slot" @hover="emit('hover', $event)" />
                    <svg class="arrow" viewBox="0 0 11 7" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M0 3h7v-3h1v1h1v1h1v1h1v1h-1v1h-1v1h-1v1h-1v-3h-7z" />
                    </svg>
                    <Slot result :stack="preview" data-list="craftpreview" data-index="0"
                        @press="(b, s, e) => carry.slot({ list: 'craftpreview', index: 0 }, preview, b, s, e)"
                        @hover="(inside) => emit('hover', inside ? { slot: { list: 'craftpreview', index: 0 }, stack: preview } : null)" />
                </div>
            </div>
        </section>
        <SlotGrid list="main" :stacks="lists.main.stacks" :columns="row" :start="row" :held="carry.held.value"
            @press="carry.slot" @hover="emit('hover', $event)" />
        <SlotGrid list="main" :stacks="lists.main.stacks" :columns="row" :start="0" :end="row" :held="carry.held.value"
            class="hotbar" @press="carry.slot" @hover="emit('hover', $event)" />
    </div>
</template>

<style scoped>
.survival { display: flex; flex-direction: column; gap: calc(var(--u) * 3); }
.top { display: flex; gap: calc(var(--u) * 6); align-items: stretch; }
.figure { width: calc(var(--u) * 52); height: calc(var(--u) * 56); display: grid; place-items: center; border: var(--b) solid var(--edge);
    background: var(--stone-0); box-shadow: var(--sunk); }
.figure span { font-size: 10px; color: var(--ink-faint); text-align: center; padding: 6px; }
.crafting { display: flex; flex-direction: column; justify-content: center; gap: calc(var(--u) * 2); }
.label { font: 11px var(--font-display); text-transform: uppercase; color: var(--ink-dim); }
.craft-row { display: flex; align-items: center; gap: calc(var(--u) * 6); }
.arrow { width: calc(var(--u) * 11); height: calc(var(--u) * 7); color: var(--stone-4); }
.hotbar { margin-top: calc(var(--u) * 2); }
</style>
