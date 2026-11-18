/**
 * The shared table's view model. The grid underneath (AG Grid) is this
 * folder's business: nothing outside it imports the grid or its types.
 */
export interface Column<T> {
    id: string
    header: string
    value: (row: T) => string
    /** Fixed width in px; leave it out and set flex for a fluid column. */
    width?: number
    flex?: number
    /** The least a fluid column narrows to; past it the table scrolls sideways. */
    minWidth?: number
    mono?: boolean
    muted?: boolean
    /** This row's text colour in this column. */
    color?: (row: T) => string | undefined
    /** A colour chip before the value: what the row is drawn as elsewhere (a chart's series). */
    swatch?: (row: T) => string
}

/** Where a feed delivers rows to a table, as they come and go. */
export interface TableFeedSink<T> {
    add(rows: T[]): void
    remove(rows: T[]): void
    clear(): void
}

export interface TableFeedHandle<T> {
    /** Counts and flags for a toolbar: reactive. */
    readonly state: { total: number; received: number; buffered: number; paused: boolean; following: boolean }
    append(rows: readonly T[]): void
    clear(): void
    setPaused(paused: boolean): void
    setFollowing(following: boolean): void
    /** An owned copy of what's kept. */
    snapshot(): T[]
    subscribe(sink: TableFeedSink<T>): () => void
}
