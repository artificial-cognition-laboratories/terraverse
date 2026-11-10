import { MockBackend } from "./mock.ts"
import { TauriBackend } from "./tauri.ts"
import type { Backend } from "./types.ts"

export * from "./types.ts"

/** The mock in a plain browser, or anywhere when VITE_BACKEND=mock (`bun run dev:mock`). */
export function connect(): Backend {
    const inTauri = "__TAURI_INTERNALS__" in window
    return !inTauri || import.meta.env.VITE_BACKEND === "mock" ? MockBackend() : TauriBackend()
}
