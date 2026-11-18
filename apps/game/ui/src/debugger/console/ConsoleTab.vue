<script setup lang="ts">
import { computed, onUnmounted, ref, watch } from "vue"
import type { LogLine } from "../../bridge/index.ts"
import DataTable from "../../table/DataTable.vue"
import type { Column, TableFeedHandle } from "../../table/types.ts"

/**
 * The engine's log, as a browser's console shows a page's: each line whole
 * (wrapped, never cut), tinted by its level, a filter, and the newest line
 * followed until you scroll up.
 */
const props = defineProps<{ feed: TableFeedHandle<LogLine> }>()

const text = ref("")
const search = ref("")
let typing: ReturnType<typeof setTimeout> | undefined
watch(text, (value) => {
    clearTimeout(typing)
    typing = setTimeout(() => (search.value = value.trim().toLowerCase()), 120)
})
onUnmounted(() => clearTimeout(typing))

const filter = computed(() => {
    const needle = search.value
    return (line: LogLine) => !needle || line.text.toLowerCase().includes(needle) || line.thread.toLowerCase().includes(needle)
})
const shown = ref(0)

const columns: Column<LogLine>[] = [
    { id: "text", header: "Message", flex: 1, mono: true, wrap: true, value: (line) => line.text },
]
</script>

<template>
    <div class="console">
        <div class="toolbar">
            <label class="search">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M2 3h12l-4.5 5.5V13l-3-1.5v-3z" /></svg>
                <input v-model="text" type="search" placeholder="Filter Output" aria-label="Filter output" spellcheck="false" />
            </label>
            <span class="count" :title="`${shown.toLocaleString()} shown · ${props.feed.state.total.toLocaleString()} kept`">
                {{ shown.toLocaleString() }} / {{ props.feed.state.total.toLocaleString() }}
            </span>
            <button class="icon" :class="{ on: props.feed.state.following }" title="Follow the newest" aria-label="Follow the newest"
                @click="props.feed.setFollowing(!props.feed.state.following)">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5v10M3.5 8.5l4.5 4.5 4.5-4.5M3 14.5h10" /></svg>
            </button>
        </div>
        <DataTable class="lines" :columns="columns" :feed="props.feed" :filter="filter" :row-key="(line) => String(line.seq)"
            :row-class="(line) => `lvl-${line.level}`" headless @filtered="(n) => (shown = n)" />
    </div>
</template>

<style scoped>
.console { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.toolbar { display: flex; align-items: center; gap: 4px; flex: none; height: 32px; padding: 0 4px; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }

button { display: flex; align-items: center; border: 0; background: none; color: var(--dt-dim); font: inherit; cursor: default; }
.icon { justify-content: center; flex: none; width: 26px; height: 26px; border-radius: 4px; }
.icon:hover { color: var(--dt-ink); background: var(--dt-hover); }
.icon.on { color: var(--dt-accent); }
svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }

.search { display: flex; flex: 1; align-items: center; gap: 6px; min-width: 0; height: 24px; padding: 0 6px; border-radius: 4px; color: var(--dt-faint); }
.search:focus-within { background: var(--dt-hover); outline: 1px solid var(--dt-accent); }
input { flex: 1; min-width: 0; border: 0; outline: 0; background: none; color: var(--dt-ink); font: inherit; }
input::placeholder { color: var(--dt-faint); }
input::-webkit-search-cancel-button { filter: invert(0.7); }
.count { flex: none; padding: 0 4px; color: var(--dt-dim); font: 11px ui-monospace, "JetBrains Mono", monospace; white-space: nowrap; }


.lines { flex: 1; min-height: 0; }
.lines :deep(.lvl-error) { background: var(--dt-error-bg); color: var(--dt-error); }
.lines :deep(.lvl-warning) { background: var(--dt-warn-bg); color: var(--dt-warn); }
.lines :deep(.lvl-action) { color: var(--dt-action); }
.lines :deep(.lvl-error .cell-muted), .lines :deep(.lvl-warning .cell-muted) { color: inherit; opacity: 0.75; }
</style>
