import { themeQuartz } from "ag-grid-community"

/**
 * The grid in a look of the page it's on. Colours come from the host's CSS
 * variables, so one table serves the debugger's devtools and, later, the game.
 */
export const TABLE_THEMES = {
    devtools: themeQuartz.withParams({
        backgroundColor: "var(--dt-bg)",
        foregroundColor: "var(--dt-ink)",
        headerBackgroundColor: "var(--dt-bg)",
        headerTextColor: "var(--dt-dim)",
        headerFontSize: 11,
        headerFontWeight: 600,
        borderColor: "var(--dt-line)",
        rowBorder: { color: "var(--dt-row-line)" },
        rowHoverColor: "var(--dt-hover)",
        selectedRowBackgroundColor: "var(--dt-held)",
        fontFamily: "ui-monospace, \"JetBrains Mono\", Menlo, monospace",
        fontSize: 12,
        rowHeight: 22,
        headerHeight: 24,
        cellHorizontalPaddingScale: 0.5,
        columnBorder: false,
        wrapperBorder: false,
        wrapperBorderRadius: 0,
    }),
} as const

export type TableTheme = keyof typeof TABLE_THEMES
