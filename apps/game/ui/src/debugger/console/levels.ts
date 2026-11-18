import type { LogLevel } from "../../bridge/index.ts"

/** The Console's levels, in toolbar order: each a toggle, and a tint for its rows. */
export const LEVELS = [
    { id: "error", label: "Errors" },
    { id: "warning", label: "Warnings" },
    { id: "action", label: "Actions" },
    { id: "info", label: "Info" },
] as const satisfies readonly { id: LogLevel; label: string }[]
