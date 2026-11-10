import { inject } from "vue"
import { STORE } from "./store.ts"
import type { LauncherStore } from "./store.ts"

export function useStore(): LauncherStore {
    const store = inject(STORE)
    if (!store) throw new Error("no launcher store provided")
    return store
}

const UNITS = ["B", "KB", "MB", "GB", "TB"]
export function bytes(n: number): string {
    let i = 0
    while (n >= 1024 && i < UNITS.length - 1) { n /= 1024; i++ }
    return `${n.toFixed(i < 3 ? 0 : 1)} ${UNITS[i]}`
}

export function date(iso: string): string {
    return new Date(`${iso}T00:00:00`).toLocaleDateString(undefined, { day: "numeric", month: "short", year: "numeric" })
}

export function since(ms: number, now = Date.now()): string {
    const minutes = Math.floor((now - ms) / 60_000)
    if (minutes < 1) return "just now"
    if (minutes < 60) return `${minutes} min`
    const hours = Math.floor(minutes / 60)
    if (hours < 48) return `${hours} h ${minutes % 60} min`
    return `${Math.floor(hours / 24)} days`
}
