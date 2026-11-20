<script setup lang="ts" generic="T">
import { AgGridVue } from "ag-grid-vue3"
import { AllCommunityModule, ModuleRegistry, type ColDef, type GridApi, type IRowNode, type RowClassParams, type RowClickedEvent } from "ag-grid-community"
import { computed, onUnmounted, watch } from "vue"
import { TABLE_THEMES, type TableTheme } from "./theme.ts"
import type { Column, TableFeedHandle } from "./types.ts"

ModuleRegistry.registerModules([AllCommunityModule])

/**
 * The one table: virtualised, so a stream of tens of thousands of rows stays
 * smooth. Rows come fixed (`rows`) or streaming (`feed`), never both.
 */
const props = withDefaults(defineProps<{
    columns: Column<T>[]
    rows?: T[]
    feed?: TableFeedHandle<T>
    filter?: (row: T) => boolean
    rowKey?: (row: T) => string
    /** A class for the whole row (a level's tint). */
    rowClass?: (row: T) => string | undefined
    theme?: TableTheme
    /** No header row: for a list that reads on its own, like a console. */
    headless?: boolean
}>(), { theme: "devtools", headless: false })

const emit = defineEmits<{
    select: [row: T]
    /** How many rows show after the filter. */
    filtered: [count: number]
}>()

const columnDefs = computed<ColDef<T>[]>(() => props.columns.map((column) => ({
    colId: column.id,
    headerName: column.header,
    valueGetter: (params) => (params.data ? column.value(params.data) : ""),
    width: column.width,
    flex: column.flex,
    minWidth: column.minWidth,
    wrapText: column.wrap,
    autoHeight: column.wrap,
    suppressSizeToFit: column.width !== undefined,
    cellClass: [column.mono ? "cell-mono" : "", column.muted ? "cell-muted" : ""].filter(Boolean),
    cellRenderer: column.swatch ? (params: { data?: T; value: string }) => swatched(params, column) : undefined,
    cellStyle: column.color
        ? (params) => ({ color: (params.data && column.color?.(params.data)) || "inherit" })
        : undefined,
})))

// A chip and the value, built as nodes: the value is the row's text, never markup.
function swatched(params: { data?: T; value: string }, column: Column<T>) {
    const cell = document.createElement("span")
    if (params.data && column.swatch) {
        const chip = document.createElement("i")
        chip.className = "cell-swatch"
        chip.style.background = column.swatch(params.data)
        cell.append(chip)
    }
    cell.append(params.value)
    return cell
}

const defaultColDef: ColDef<T> = { sortable: false, resizable: true, filter: false }
const getRowId = computed(() => {
    const key = props.rowKey
    return key ? (params: { data: T }) => key(params.data) : undefined
})
const onRowClicked = (event: RowClickedEvent<T>) => {
    if (event.data) emit("select", event.data)
}
const counted = (event: { api: GridApi<T> }) => emit("filtered", event.api.getDisplayedRowCount())
const getRowClass = (params: RowClassParams<T>) => (params.data && props.rowClass?.(params.data)) || undefined

let api: GridApi<T> | null = null
let unsubscribe: (() => void) | undefined
// Following scrolls programmatically, which fires the same scroll event a
// person's does: the next one is skipped so it isn't read as scrolling away.
let ignoreNextScroll = false
let scrollFrame: number | null = null

function onGridReady(params: { api: GridApi<T> }) {
    api = params.api
    api.setGridOption("rowData", props.feed ? props.feed.snapshot() : (props.rows ?? []))
    const feed = props.feed
    if (!feed) return
    if (feed.state.following) scrollToBottom()
    unsubscribe = feed.subscribe({
        add: (rows) => transact({ add: rows }),
        remove: (rows) => transact({ remove: rows }),
        clear: () => api?.setGridOption("rowData", []),
    })
}

function scrollToBottom() {
    if (!api) return
    const last = api.getDisplayedRowCount() - 1
    if (last < 0) return
    ignoreNextScroll = true
    api.ensureIndexVisible(last, "bottom")
}

// The grid batches transactions itself; following scrolls once a frame, not
// once a transaction: a forced layout per log line is what freezes a busy log.
function transact(transaction: { add?: T[]; remove?: T[] }) {
    api?.applyTransactionAsync(transaction, () => {
        if (scrollFrame !== null || !props.feed?.state.following) return
        scrollFrame = requestAnimationFrame(() => {
            scrollFrame = null
            if (props.feed?.state.following) scrollToBottom()
        })
    })
}

function atBottom() {
    if (!api) return true
    const last = api.getDisplayedRowCount() - 1
    if (last < 0) return true
    // the last row's own bottom: rows that wrap aren't all one height
    const row = api.getDisplayedRowAtIndex(last)
    const bottom = (row?.rowTop ?? 0) + (row?.rowHeight ?? 22)
    return bottom - api.getVerticalPixelRange().bottom < 4
}

function onBodyScroll() {
    if (!props.feed) return
    if (ignoreNextScroll) {
        ignoreNextScroll = false
        return
    }
    props.feed.setFollowing(atBottom())
}

watch(() => props.rows, (rows) => {
    if (!props.feed) api?.setGridOption("rowData", rows ?? [])
})
watch(() => props.filter, () => {
    api?.onFilterChanged()
    if (props.feed?.state.following) scrollToBottom()
})
// Asked to follow again (the toolbar): go to the end now, not at the next row.
watch(() => props.feed?.state.following, (following) => {
    if (following) scrollToBottom()
})

onUnmounted(() => {
    unsubscribe?.()
    if (scrollFrame !== null) cancelAnimationFrame(scrollFrame)
    api = null
})
</script>

<template>
    <AgGridVue class="data-table" :theme="TABLE_THEMES[theme]" :column-defs="columnDefs" :default-col-def="defaultColDef"
        :get-row-id="getRowId" :get-row-class="getRowClass" :animate-rows="false"
        :row-selection="{ mode: 'singleRow', checkboxes: false }" :header-height="headless ? 0 : undefined" :is-external-filter-present="() => filter !== undefined"
        :does-external-filter-pass="(node: IRowNode<T>) => (node.data && filter ? filter(node.data) : true)" suppress-cell-focus
        @grid-ready="onGridReady" @row-clicked="onRowClicked" @filter-changed="counted" @model-updated="counted"
        @body-scroll="onBodyScroll" />
</template>

<style scoped>
.data-table { width: 100%; height: 100%; min-height: 0; }
</style>

<style>
.data-table .ag-header-cell-label { justify-content: flex-start; }
.data-table .cell-mono { font-family: ui-monospace, "JetBrains Mono", Menlo, monospace; }
.data-table .cell-muted { color: var(--dt-dim); }
/* a wrapping cell: its text breaks where it must, at the grid's own line height */
.data-table .ag-cell-wrap-text { padding-top: 3px; padding-bottom: 3px; line-height: 16px; word-break: break-word; white-space: pre-wrap; }
.data-table .cell-swatch { display: inline-block; width: 8px; height: 8px; margin-right: 7px; border-radius: 2px; vertical-align: 0; }
/* No grid animations: in a tool they read as lag. */
.data-table *, .data-table *::before, .data-table *::after { animation: none !important; transition: none !important; }
</style>
