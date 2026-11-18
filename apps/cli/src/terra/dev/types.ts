import type { TerraError } from "../../errors"
import type { EngineMode } from "../engine"
import type { PreparedServer } from "../project"

/** How a dev session runs. */
export type DevOptions = {
    readonly mode: EngineMode
    readonly port: number
    /** A save made new every run, never the server's own. */
    readonly fresh: boolean
    /** For a mod: run inside this server, with its rules and a copy of its save. */
    readonly in: string | null
}

/** What a dev session reports as it goes, for whoever shows it. */
export type DevEvent =
    | { readonly kind: "prepared"; readonly prepared: PreparedServer; readonly ms: number; readonly first: boolean }
    | { readonly kind: "failed"; readonly error: TerraError }
    | { readonly kind: "engine"; readonly line: string }
    | { readonly kind: "restarting"; readonly why: "content" | "config" }
    | { readonly kind: "stopped"; readonly code: number }
    | { readonly kind: "closed" }
    | { readonly kind: "waiting" }
