<script setup lang="ts">
import { computed } from "vue"
import type { Stack } from "../../bridge/index.ts"
import "../cell.css"
import { formatMass } from "../mass.ts"
import { useStore } from "../store.ts"
import { CATALOGUE, HOLD } from "./catalogue.ts"
import FormIcon from "./FormIcon.vue"
import { formatTime, making, type Offer, type Thing } from "./making.ts"

/**
 * What the crafting grid's things could become, worked by what's on the cursor: icons,
 * one wide, no words (the tooltip has them). One the cursor can't do is dim, badged with what to hold.
 */
const props = defineProps<{ grid: readonly (Stack | null)[]; carried: Stack | null }>()
const emit = defineEmits<{ hover: [over: { note: string } | null]; pick: [offer: Offer] }>()
const store = useStore()

// a game item as a thing: one of the stack, as the catalogue reads it
function thing(stack: Stack | null): Thing | null {
    const known = stack ? CATALOGUE.items[stack.item] : undefined
    return stack && known ? { ...known, mass: store.items[stack.item]?.mass ?? 1 } : null
}
const offers = computed(() => {
    const grid = props.grid.filter((s): s is Stack => s !== null)
    const things = grid.map(thing)
    // something in the grid the catalogue doesn't know: nothing's offered for it
    if (!grid.length || things.some((t) => t === null)) return []
    return making.offers(CATALOGUE, things as Thing[], thing(props.carried))
})
const name = (t: Thing) => `${CATALOGUE.materials[t.material]?.name} ${CATALOGUE.forms[t.form]?.name.toLowerCase()}`
function note(o: Offer) {
    const what = `${name(o.thing)} · ${formatMass(o.thing.mass, true)} · ${formatTime(o.seconds)}`
    return o.needs ? `${what}\nHold ${CATALOGUE.forms[HOLD[o.needs].form]?.name.toLowerCase()} to ${o.process.verb.toLowerCase()} it` : what
}
</script>

<template>
    <div class="offers" :class="{ empty: !offers.length }">
        <button v-for="o in offers" :key="o.key" class="item-cell" :class="{ pickable: !o.needs, dim: o.needs }"
            @click="!o.needs && emit('pick', o)" @mouseenter="emit('hover', { note: note(o) })" @mouseleave="emit('hover', null)">
            <FormIcon :form="o.thing.form" :material="o.thing.material" />
            <span v-if="o.needs" class="badge"><FormIcon :form="HOLD[o.needs].form" :material="HOLD[o.needs].material" /></span>
        </button>
    </div>
</template>

<style scoped>
/* a column of cells, three seen; the wheel scrolls the rest, no bar */
.offers { display: flex; flex-direction: column; gap: 0.25rem; max-height: calc(2.75rem * 3 + 0.25rem * 2); overflow-y: auto; scrollbar-width: none; }
.offers::-webkit-scrollbar { display: none; }
.offers > * { flex: none; }
.offers.empty { border: 1px dashed rgb(231 237 244 / 0.06); border-radius: var(--radius); width: 2.75rem; height: 2.75rem; }
.dim :deep(.form-icon) { opacity: 0.3; }
.badge { position: absolute; right: 0.1rem; bottom: 0.1rem; --u: 0.06rem; }
.badge :deep(.form-icon) { opacity: 1; }
</style>
