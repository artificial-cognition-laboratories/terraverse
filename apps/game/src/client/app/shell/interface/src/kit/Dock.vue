<script setup lang="ts" generic="T extends string">
import { onBeforeUnmount, onMounted, provide, ref, toRef } from "vue"
import IconRail from "./IconRail.vue"
import RailBack from "./RailBack.vue"
import SearchBox from "./SearchBox.vue"
import { WORKBENCH_SEARCH } from "./workbench.ts"

/**
 * A screen over the universe (planning/sprint-36-menu-universe.md): a sheet of
 * glass docked down the left, the whole height (a rail of groups, a bar with
 * where you are and the way out, search, the list or a workspace, a footer
 * action), as the Library's Screen lays its rail and bar; and a card on the right (`content`).
 * The rest is the planet behind. `covers` says how much of the window's
 * left and right the two take (page pixels), so the planet is framed in
 * what's left (usePlanetarium's `frame`). The same props and slots as
 * WorkbenchPanel, so a screen moves between the two whole.
 */
const props = defineProps<{
    title: string
    section: string
    groups: readonly { id: T; label: string; shortLabel?: string; rows: readonly string[]; palette: Record<string, string> }[]
    railLabel: string
    searchLabel?: string
    searchPlaceholder?: string
    /** The panel's width (CSS): wider for a workspace of controls. */
    width?: string
    /** The way out, at the rail's foot as on every full screen: its label (none, no way out there). */
    back?: string
}>()
defineEmits<{ back: [] }>()
const group = defineModel<T>("group", { required: true })
const query = defineModel<string>("query", { default: "" })
const covers = defineModel<{ left: number; right: number }>("covers", { default: () => ({ left: 0, right: 0 }) })
provide(WORKBENCH_SEARCH, {
    query,
    label: toRef(props, "searchLabel"),
    placeholder: toRef(props, "searchPlaceholder"),
})

const dock = ref<HTMLElement | null>(null)
const card = ref<HTMLElement | null>(null)
function measure() {
    const left = dock.value?.getBoundingClientRect().right ?? 0
    const cardLeft = card.value?.getBoundingClientRect().left
    const right = cardLeft === undefined ? 0 : window.innerWidth - cardLeft
    // only a real change: a new object every time re-renders the parent, which measures again
    if (covers.value.left !== left || covers.value.right !== right) covers.value = { left: left, right: right }
}
const sizes = new ResizeObserver(measure)
onMounted(() => {
    if (dock.value) sizes.observe(dock.value)
    window.addEventListener("resize", measure)
    measure()
})
onBeforeUnmount(() => {
    sizes.disconnect()
    window.removeEventListener("resize", measure)
})
// the card comes and goes with its content (a function ref is called on
// every render: only a different element is news)
function cardRef(el: unknown) {
    const now = el instanceof HTMLElement ? el : null
    if (now === card.value) return
    if (card.value) sizes.unobserve(card.value)
    card.value = now
    if (now) sizes.observe(now)
    measure()
}
</script>

<template>
    <div class="dock-scene">
        <div ref="dock" class="dock" :style="width ? { width: width } : undefined">
            <IconRail v-model="group" class="dock-rail" :items="groups" :label="railLabel">
                <template v-if="back" #foot><RailBack :label="back" @back="$emit('back')" /></template>
            </IconRail>
            <div class="dock-main">
                <header class="dock-bar">
                    <h2><span class="chev">»</span> {{ title }}<template v-if="section">&nbsp;<span class="sep">/</span>&nbsp;<span class="section">{{ section }}</span></template></h2>
                    <div v-if="$slots.actions" class="actions"><slot name="actions" /></div>
                </header>
                <slot v-if="$slots.workspace" name="workspace" />
                <template v-else>
                    <div v-if="searchLabel" class="dock-search">
                        <SearchBox v-model="query" :label="searchLabel" :placeholder="searchPlaceholder" />
                    </div>
                    <div class="dock-list"><slot name="list" /></div>
                </template>
                <footer v-if="$slots.footer" class="dock-foot"><slot name="footer" /></footer>
            </div>
        </div>
        <aside v-if="$slots.content" :ref="cardRef" class="dock-card"><slot name="content" /></aside>
    </div>
</template>

<style scoped>
.dock-scene { position: fixed; inset: 0; pointer-events: none; }
.dock, .dock-card { pointer-events: auto; }
/* the page can't blur the planet under it (it's under the page): strong glass instead */
.dock { position: absolute; top: 0; bottom: 0; left: 0; display: grid; grid-template-columns: 84px minmax(0, 1fr); width: min(460px, 42vw); border-right: 1px solid var(--glass-edge); background: var(--glass-strong); }
.dock-rail { border-right: 1px solid var(--glass-edge); background: rgb(4 7 11 / 0.5); }
.dock-main { display: flex; flex-direction: column; min-width: 0; min-height: 0; }
.dock-bar { display: flex; align-items: center; gap: 1rem; flex: none; height: 3.6rem; padding: 0 1rem 0 1.4rem; border-bottom: 1px solid var(--glass-edge); }
h2 { flex: 1; min-width: 0; margin: 0; overflow: hidden; font: 400 0.8rem var(--font-display); letter-spacing: 0.08em; text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.chev { color: var(--accent-hi); }
.sep { color: var(--ink-faint); }
.section { color: var(--ink-dim); }
.actions { display: flex; gap: 0.5rem; }
.dock-search { flex: none; padding: 1rem 1rem 0.5rem; }
.dock-list { flex: 1; min-height: 0; overflow-y: auto; padding: 0 0.5rem 1rem; }
.dock-foot { flex: none; display: flex; gap: 0.5rem; padding: 1rem; border-top: 1px solid var(--glass-edge); }
.dock-foot > :deep(*) { flex: 1; }
.dock-card {
    position: absolute; top: 0; right: 0; bottom: 0; width: min(380px, 30vw); overflow-y: auto;
    padding: 1.4rem; border-left: 1px solid var(--glass-edge); background: var(--glass-strong);
}
</style>
