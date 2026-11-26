<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from "vue"

/** One row: `label` its words (`marks` the indices lit as typed), `detail` beside it; a new `group`
 *  draws a line above it. `dim`: shown, not offered. */
export type PaletteItem = { id: string; label: string; marks?: readonly number[]; detail?: string; group?: string; dim?: boolean }

/**
 * A command palette (VS Code's quick pick): a line to type in, and under it what that line could
 * mean. Arrows choose, Enter takes the chosen (`pick`, "enter"), Tab completes it ("tab"), Esc or
 * a click off it closes. The list is the caller's: it reads `query` and hands back `items`.
 * `note` is said where there's nothing to choose.
 */
const props = defineProps<{ items: readonly PaletteItem[]; placeholder?: string; note?: string }>()
const emit = defineEmits<{ pick: [item: PaletteItem, how: "enter" | "tab" | "click"]; close: [] }>()
const query = defineModel<string>("query", { required: true })

const input = ref<HTMLInputElement | null>(null)
const list = ref<HTMLElement | null>(null)
const active = ref(0) // the keys' choice: Enter takes it
const hovered = ref(-1) // under the mouse: a tint, nothing chosen
// a new list chooses its first again; the same list, as it's typed into, keeps the place
watch(() => props.items.map((item) => item.id).join("\n"), () => { active.value = 0 })
onMounted(() => input.value?.focus({ preventScroll: true }))
defineExpose({ focus: () => input.value?.focus() })
// the line keeps the keyboard while it's open (a click on the corner's icons, say): the page then has
// every key, Esc too, and the engine never closes it behind the page's back
function refocus() { void nextTick(() => input.value?.focus({ preventScroll: true })) }

const chosen = computed(() => props.items[active.value] ?? null)
// a line between groups, where there's more than one
const grouped = computed(() => new Set(props.items.map((item) => item.group)).size > 1)
function move(by: number) {
    const count = props.items.length
    if (count === 0) return
    active.value = (active.value + by + count) % count
    void nextTick(() => list.value?.querySelector(".on")?.scrollIntoView({ block: "nearest" }))
}
function key(e: KeyboardEvent) {
    if (e.key === "ArrowDown") move(1)
    else if (e.key === "ArrowUp") move(-1)
    else if (e.key === "PageDown") move(8)
    else if (e.key === "PageUp") move(-8)
    else if (e.key === "Enter" && chosen.value && !chosen.value.dim) emit("pick", chosen.value, "enter")
    else if (e.key === "Tab" && chosen.value && !chosen.value.dim) emit("pick", chosen.value, "tab")
    else if (e.key === "Escape") emit("close")
    else return
    e.preventDefault()
}
// the label in runs, lit where it matched
function runs(item: PaletteItem) {
    const lit = new Set(item.marks ?? [])
    const out: { text: string; lit: boolean }[] = []
    for (let i = 0; i < item.label.length; i++) {
        const on = lit.has(i)
        const last = out[out.length - 1]
        if (last && last.lit === on) last.text += item.label[i]
        else out.push({ text: item.label[i]!, lit: on })
    }
    return out
}
</script>

<template>
    <div class="palette-scene" @mousedown.self="emit('close')">
        <div class="palette" role="dialog" aria-label="Command palette">
            <label class="line">
                <input ref="input" v-model="query" type="text" :placeholder="placeholder" spellcheck="false"
                    autocomplete="off" role="combobox" aria-expanded="true" @keydown="key" @blur="refocus">
            </label>
            <ul v-if="items.length" ref="list" class="list" role="listbox">
                <li v-for="(item, i) in items" :key="item.id" role="option" :aria-selected="i === active"
                    :class="{ on: i === active, over: i === hovered, dim: item.dim, first: grouped && item.group !== items[i - 1]?.group && i > 0 }"
                    @mousemove="hovered = i" @mouseleave="hovered = -1" @mousedown.prevent="!item.dim && emit('pick', item, 'click')">
                    <span class="label"><span v-for="(run, r) in runs(item)" :key="r" :class="{ lit: run.lit }">{{ run.text }}</span></span>
                    <span v-if="item.detail" class="detail">{{ item.detail }}</span>
                </li>
            </ul>
            <p v-else-if="note" class="note">{{ note }}</p>
        </div>
    </div>
</template>

<style scoped>
/* typed, never pointed at: no cursor over the game while it's open */
.palette-scene { position: fixed; inset: 0; display: flex; justify-content: center; align-items: flex-start; padding-top: 1.25rem; cursor: none; }
/* the inventory's glass (game/inventory/InventoryScreen.vue .content): the night at 0.72, frosted by
   the shell, one hairline; a little wider than the hotbar under it */
.palette { display: flex; flex-direction: column; gap: 0.25rem; width: min(32rem, calc(100vw - 4rem)); max-height: 60vh; padding: 0.25rem; border: 1px solid var(--glass-edge); border-radius: var(--radius);
    /* and the slots' faint lift over it (cell.css), as the inventory reads: grey glass, not the full screens' night */
    background: linear-gradient(rgb(231 237 244 / 0.035), rgb(231 237 244 / 0.035)), rgb(4 7 11 / 0.72); }
/* its search cell: the slots' hairline and lift, lit while typed in (it always is) */
.line { display: flex; align-items: center; gap: 0.75rem; flex: none; padding: 0 0.75rem; height: 2.5rem; border: 1px solid var(--edge-lit); border-radius: var(--radius); background: rgb(231 237 244 / 0.025); }
input { flex: 1; min-width: 0; height: 100%; border: 0; outline: 0; background: none; cursor: none; font: 0.85rem var(--font); color: var(--ink); }
input::placeholder { color: var(--ink-faint); }
.list { flex: 1; min-height: 0; overflow-y: auto; margin: 0; padding: 0; list-style: none; }
li { position: relative; display: flex; align-items: center; gap: 0.6rem; height: 1.9rem; padding: 0 0.75rem; cursor: none; color: var(--ink); font: 0.8rem var(--font); white-space: nowrap; }
li.first { margin-top: 0.3rem; border-top: 1px solid var(--glass-edge); }
/* as VS Code: the mouse tints grey glass, the keys' choice is the ocean */
li.over { background: rgb(231 237 244 / 0.06); }
li.on { background: rgb(10 75 145 / 0.55); }
li.dim { color: var(--ink-faint); }
.label { flex: none; max-width: 55%; overflow: hidden; text-overflow: ellipsis; }
.lit { color: var(--accent-hi); font-weight: 700; }
.detail { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; color: var(--ink-faint); font: 0.75rem var(--font-sans); }
.note { margin: 0; padding: 0.5rem 0.75rem; color: var(--ink-dim); font: 0.8rem var(--font-sans); }
</style>
