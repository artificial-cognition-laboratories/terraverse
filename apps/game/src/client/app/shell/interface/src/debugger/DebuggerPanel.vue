<script setup lang="ts">
import { computed, nextTick, onMounted, onUnmounted, ref } from "vue"
import type { DockSide, LogLine } from "../bridge/index.ts"
import type { TableFeedHandle } from "../table/types.ts"
import ConsoleTab from "./console/ConsoleTab.vue"
import PerfTab from "./perf/PerfTab.vue"
import type { PerfStoreHandle } from "./perf/store.ts"
import NetworkTab from "./net/NetworkTab.vue"
import type { NetStoreHandle } from "./net/store.ts"
import ServerTab from "./server/ServerTab.vue"
import type { ServerStoreHandle } from "./server/store.ts"
import Menu, { type MenuItem } from "./Menu.vue"
import { TABS, type TabId } from "./tabs.ts"

/**
 * The debugger, filling its page beside the game. A tool rather than part of
 * the game, so it dresses as a browser's devtools do, not in the game's kit:
 * a dark rail of labelled tabs, those that don't fit folded behind », and its
 * own buttons at the end.
 */
const props = defineProps<{ side: DockSide; log: TableFeedHandle<LogLine>; perf: PerfStoreHandle; net: NetStoreHandle; server: ServerStoreHandle; exportTrace: () => Promise<string> }>()
const emit = defineEmits<{ dock: [side: DockSide]; grab: []; close: [] }>()
const tab = ref<TabId>(TABS[0].id)
const shown = computed(() => TABS.find((t) => t.id === tab.value) ?? TABS[0])

// Which tabs fit: every tab's width is measured once, laid out in full, then
// the rail keeps as many as its width allows, the chosen one always among them.
const rail = ref<HTMLElement | null>(null)
const widths = ref<Map<TabId, number> | null>(null)
const room = ref(0)
const FOLD = 32 // the » button
const fits = computed<TabId[]>(() => {
    const w = widths.value
    const all = TABS.map((t) => t.id)
    if (!w) return all
    const width = (id: TabId) => {
        const px = w.get(id)
        if (px === undefined) throw new Error(`debugger: tab '${id}' was never measured`)
        return px
    }
    if (all.reduce((sum, id) => sum + width(id), 0) <= room.value) return all
    const kept = new Set<TabId>([tab.value])
    let used = width(tab.value)
    for (const id of all) {
        if (kept.has(id)) continue
        if (used + width(id) > room.value - FOLD) break
        kept.add(id)
        used += width(id)
    }
    return all.filter((id) => kept.has(id))
})
const folded = computed(() => TABS.filter((t) => !fits.value.includes(t.id)))

let watcher: ResizeObserver | null = null
onMounted(async () => {
    await document.fonts.ready
    await nextTick()
    const el = rail.value
    if (!el) throw new Error("debugger: the tab rail isn't mounted")
    const buttons = Array.from(el.querySelectorAll<HTMLElement>("[role=tab]"))
    widths.value = new Map(TABS.map((t, i) => {
        const button = buttons[i]
        if (!button) throw new Error(`debugger: no button for tab '${t.id}'`)
        return [t.id, button.offsetWidth]
    }))
    watcher = new ResizeObserver(() => (room.value = el.clientWidth))
    watcher.observe(el)
    room.value = el.clientWidth
})
onUnmounted(() => watcher?.disconnect())

const open = ref<"fold" | "more" | null>(null)
const toggle = (menu: "fold" | "more") => (open.value = open.value === menu ? null : menu)

const DOCK_BOTTOM = "M1.5 2.5h13v11h-13zM1.5 9.5h13"
const DOCK_RIGHT = "M1.5 2.5h13v11h-13zM10 2.5v11"
type More = "bottom" | "right" | "close"
const more = computed<MenuItem<More>[]>(() => [
    { id: "bottom", label: "Dock to Bottom", icon: DOCK_BOTTOM, checked: props.side === "bottom" },
    { id: "right", label: "Dock to Right", icon: DOCK_RIGHT, checked: props.side === "right" },
    { rule: true },
    { id: "close", label: "Close", shortcut: "F3" },
])
function pickMore(id: More) {
    open.value = null
    if (id === "close") emit("close")
    else if (id !== props.side) emit("dock", id)
}
function pickFolded(id: TabId) {
    open.value = null
    tab.value = id
}
</script>

<template>
    <div class="devtools" :class="side">
        <div class="edge" role="separator" :aria-orientation="side === 'right' ? 'vertical' : 'horizontal'"
            @pointerdown.left.prevent="emit('grab')" />
        <div class="pane">
            <header>
                <nav ref="rail" role="tablist" aria-label="Debugger">
                    <template v-for="t in TABS" :key="t.id">
                        <button v-if="fits.includes(t.id)" role="tab" :aria-selected="t.id === tab" :class="{ on: t.id === tab }"
                            @click="tab = t.id">
                            <svg viewBox="0 0 16 16" aria-hidden="true"><path :d="t.icon" /></svg>
                            <span>{{ t.label }}</span>
                        </button>
                    </template>
                    <div v-if="folded.length" class="hang">
                        <button class="tool dt-opener" :class="{ held: open === 'fold' }" title="More tabs" aria-label="More tabs"
                            @click="toggle('fold')">
                            <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 4l4 4-4 4M8.5 4l4 4-4 4" /></svg>
                        </button>
                        <Menu v-if="open === 'fold'" class="left" :items="folded.map((t) => ({ id: t.id, label: t.label, icon: t.icon }))"
                            @pick="pickFolded" @close="open = null" />
                    </div>
                </nav>
                <div class="tools">
                    <button class="tool" :title="side === 'right' ? 'Dock to Bottom' : 'Dock to Right'"
                        :aria-label="side === 'right' ? 'Dock to Bottom' : 'Dock to Right'"
                        @click="emit('dock', side === 'right' ? 'bottom' : 'right')">
                        <svg viewBox="0 0 16 16" aria-hidden="true"><path :d="side === 'right' ? DOCK_BOTTOM : DOCK_RIGHT" /></svg>
                    </button>
                    <div class="hang">
                        <button class="tool dt-opener" :class="{ held: open === 'more' }" title="Customize" aria-label="Customize"
                            @click="toggle('more')">
                            <svg viewBox="0 0 16 16" aria-hidden="true" class="dots"><path d="M3 8h.01M8 8h.01M13 8h.01" /></svg>
                        </button>
                        <Menu v-if="open === 'more'" :items="more" checks @pick="pickMore" @close="open = null" />
                    </div>
                    <button class="tool" title="Close (F3)" aria-label="Close" @click="emit('close')">
                        <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M4 4l8 8M12 4l-8 8" /></svg>
                    </button>
                </div>
            </header>
            <main>
                <ConsoleTab v-if="tab === 'console'" :feed="log" />
                <PerfTab v-else-if="tab === 'perf'" :perf="perf" :export-trace="exportTrace" />
                <NetworkTab v-else-if="tab === 'net'" :perf="perf" :net="net" />
                <ServerTab v-else-if="tab === 'server'" :frames="perf.frames.value" :server="server" />
                <div v-else class="empty">
                    <svg viewBox="0 0 16 16" aria-hidden="true"><path :d="shown.icon" /></svg>
                    <h2>{{ shown.label }}</h2>
                    <p>{{ shown.about }}</p>
                    <p class="unwired">Not wired yet: nothing reaches this tab from the engine.</p>
                </div>
            </main>
        </div>
    </div>
</template>

<style scoped>
/* Its own palette: a browser's dark devtools, not the game's stone. */
.devtools {
    --dt-rail: #1c1b22;
    --dt-bg: #232327;
    --dt-hover: #2f2e36;
    --dt-held: #3a3944;
    --dt-line: #3b3a42;
    --dt-ink: #f0f0f4;
    --dt-dim: #a6a5ad;
    --dt-faint: #74737b;
    --dt-accent: #3d9bff;
    --dt-menu: #2b2a33;
    --dt-menu-line: #4a4952;
    --dt-menu-hover: #3c3b45;
    --dt-row-line: #2c2b31;
    --dt-chip: #33323a;
    --dt-chip-line: #4a4952;
    --dt-error: #ff8f8f;
    --dt-error-bg: #3d2228;
    --dt-warn: #ffd76e;
    --dt-warn-bg: #3a321c;
    --dt-action: #8fd1ff;

    display: flex; height: 100%; min-width: 0; min-height: 0;
    background: var(--dt-bg); color: var(--dt-ink);
    font: 13px/1.4 system-ui, "Segoe UI", Roboto, sans-serif;
    user-select: none;
}
.bottom { flex-direction: column; }
.pane { flex: 1; display: flex; flex-direction: column; min-width: 0; min-height: 0; }

/* The edge to the game: a hairline, lit while held, with a wider grip than it shows. */
.edge { flex: none; position: relative; z-index: 2; background: var(--dt-line); }
.right .edge { width: 1px; cursor: col-resize; }
.bottom .edge { height: 1px; cursor: row-resize; }
.edge::after { content: ""; position: absolute; inset: -3px; }
.edge:hover, .edge:active { background: var(--dt-accent); }

header { display: flex; align-items: stretch; height: 34px; flex: none; background: var(--dt-rail); border-bottom: 1px solid var(--dt-line); }
nav { display: flex; flex: 1; min-width: 0; }
button { display: flex; align-items: center; border: 0; background: none; color: var(--dt-dim); font: inherit; cursor: default; }

[role=tab] { position: relative; flex: none; gap: 6px; padding: 0 12px 0 10px; white-space: nowrap; }
[role=tab]:hover { color: var(--dt-ink); background: var(--dt-hover); }
[role=tab].on { color: var(--dt-ink); background: var(--dt-bg); }
[role=tab].on svg { stroke: var(--dt-accent); }

.hang { position: relative; display: flex; align-items: center; padding: 0 2px; }
.tools { display: flex; flex: none; align-items: center; gap: 2px; padding: 0 4px; }
.tools > .tool:first-child { margin-left: 4px; }
.tools::before { content: ""; width: 1px; height: 18px; margin-right: 4px; background: var(--dt-line); }
.tool { justify-content: center; width: 28px; height: 28px; border-radius: 4px; }
.tool:hover { color: var(--dt-ink); background: var(--dt-hover); }
.tool.held { color: var(--dt-ink); background: var(--dt-held); }
.left { right: auto; left: 0; }

svg { width: 16px; height: 16px; flex: none; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
.dots { stroke-width: 2.6; }

main { flex: 1; min-height: 0; overflow: auto; }
main:has(.console, .perf, .network, .server) { overflow: hidden; }
.empty { display: flex; flex-direction: column; align-items: center; gap: 6px; max-width: 380px; margin: 0 auto; padding: 48px 20px; text-align: center; }
.empty svg { width: 28px; height: 28px; stroke: var(--dt-faint); stroke-width: 1; }
h2 { margin: 4px 0 0; font-size: 14px; font-weight: 600; }
p { margin: 0; color: var(--dt-dim); }
.unwired { margin-top: 6px; color: var(--dt-faint); font-style: italic; }

/* Its scrollbars too: thin and quiet, not the game's bevelled blocks. */
.devtools :deep(::-webkit-scrollbar) { width: 10px; height: 10px; }
.devtools :deep(::-webkit-scrollbar-track) { background: transparent; box-shadow: none; }
.devtools :deep(::-webkit-scrollbar-thumb) { background: #4a4952; border: 2px solid var(--dt-bg); border-radius: 5px; box-shadow: none; }
.devtools :deep(::-webkit-scrollbar-thumb:hover) { background: #5d5c66; }
</style>
