<script setup lang="ts">
import { computed, ref } from "vue"
import PeriodicTable from "../matter/PeriodicTable.vue"
import { useTable, useWiki } from "./nav.ts"

/** The periodic table across the whole page: a modelled element opens in Matter. */
const wiki = useWiki()
const table = useTable()
const element = ref<string | null>(null)
const modelled = computed(() => new Set(table.substances.filter((s) => s.kind === "element").map((s) => Object.keys(s.composition)[0]!)))
const picked = computed(() => element.value ? table.bySymbol.get(element.value) ?? null : null)
function pick(symbol: string) {
    const s = table.substances.find((x) => x.kind === "element" && x.composition[symbol])
    if (s) wiki.go("matter", s.id)
    else element.value = symbol
}
</script>

<template>
    <div class="page">
        <PeriodicTable class="periodic" :elements="table.elements" :modelled="modelled" :selected="element" @pick="pick" />
        <p v-if="picked" class="note">{{ picked.name }} · {{ picked.symbol }} · {{ picked.number }} · {{ picked.category }} · {{ picked.mass }} g/mol. Not modelled yet: lit elements have phase data.</p>
        <p v-else class="note">Lit elements are modelled: click one to see it in Matter.</p>
    </div>
</template>

<style scoped>
.page { display: flex; flex-direction: column; gap: 12px; height: 100%; padding: 18px 22px; overflow: auto; background: var(--stone-0); }
.periodic { width: 100%; }
.note { margin: 0; color: var(--ink-dim); font-size: 12px; }
</style>
