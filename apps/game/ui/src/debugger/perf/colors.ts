/**
 * A zone's colour, the same wherever it's drawn (strip, flame chart, table).
 * Names take the next colour as they're first met, so the few that make up a
 * frame never share one; idle time is drawn as idle, not as work.
 */
const PALETTE = [
    "#4f9dff", "#e5a33b", "#5fbf72", "#c678dd", "#e06c75", "#3fbfbf",
    "#d6c15c", "#8a8cf0", "#e88a5a", "#7fc8a9", "#d987b8", "#a3c94f",
] as const

/** The frame's time no outermost zone accounts for. */
export const OTHER = "Other"
export const OTHER_COLOR = "#5a5963"
/**
 * Zones that are a thread waiting, not working: drawn as idle, and left off
 * the Live charts' lines. A table, so a new wait is one line here.
 */
export const IDLE = new Set(["Wait", "Receive", "Server::yieldTo...() sleep"])
const IDLE_COLOR = "#3a4656"
const FIXED: Record<string, string> = { [OTHER]: OTHER_COLOR }

const given = new Map<string, string>()

export function colorOf(name: string): string {
    const fixed = FIXED[name]
    if (fixed) return fixed
    if (IDLE.has(name)) return IDLE_COLOR
    let color = given.get(name)
    if (!color) {
        color = PALETTE[given.size % PALETTE.length]
        if (!color) throw new Error("perf: the palette is empty")
        given.set(name, color)
    }
    return color
}
