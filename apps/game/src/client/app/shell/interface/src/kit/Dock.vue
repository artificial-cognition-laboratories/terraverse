<script setup lang="ts" generic="T extends string">
import { onBeforeUnmount, onMounted, provide, ref, toRef } from "vue"
import IconRail from "./IconRail.vue"
import Panel from "./Panel.vue"
import SearchBox from "./SearchBox.vue"
import { WORKBENCH_SEARCH } from "./workbench.ts"

/**
 * A screen over the universe (planning/sprint-36-menu-universe.md): a panel
 * floating down the left (a rail of groups, search, the list or a
 * workspace, a footer action) and a card floating on the right (`content`).
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
}>()
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
            <Panel :title="section ? `${title} / ${section}` : title" class="dock-panel">
                <template #actions>
                    <slot name="actions" />
                </template>
                <div class="dock-frame">
                    <IconRail v-model="group" :items="groups" :label="railLabel" />
                    <div class="dock-stage">
                        <slot v-if="$slots.workspace" name="workspace" />
                        <template v-else>
                            <div v-if="searchLabel" class="dock-search">
                                <SearchBox v-model="query" :label="searchLabel" :placeholder="searchPlaceholder" />
                            </div>
                            <div class="dock-list"><slot name="list" /></div>
                        </template>
                    </div>
                </div>
                <template v-if="$slots.footer" #footer><slot name="footer" /></template>
            </Panel>
        </div>
        <aside v-if="$slots.content" :ref="cardRef" class="dock-card"><slot name="content" /></aside>
    </div>
</template>

<style scoped>
.dock-scene { position: fixed; inset: 0; pointer-events: none; }
.dock, .dock-card { pointer-events: auto; }
.dock { position: absolute; top: 16px; bottom: 16px; left: 16px; display: flex; width: min(440px, 40vw); }
.dock-panel { flex: 1; min-height: 0; }
.dock-panel :deep(.body) { overflow: hidden; padding: 0; }
.dock-frame { display: grid; grid-template-columns: 66px minmax(0, 1fr); width: 100%; height: 100%; min-height: 0; }
.dock-stage { display: flex; flex-direction: column; min-width: 0; min-height: 0; background: var(--stone-0); }
.dock-search { padding: 10px; border-bottom: var(--b) solid var(--edge); }
.dock-list { flex: 1; min-height: 0; overflow-y: auto; }
.dock-card {
    position: absolute; top: 16px; right: 16px; width: min(380px, 30vw); max-height: calc(100% - 32px); overflow-y: auto;
    padding: 18px; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.9); box-shadow: var(--raised), 8px 8px 0 rgb(0 0 0 / 0.35);
}
</style>
