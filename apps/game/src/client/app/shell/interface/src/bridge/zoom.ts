import type { BridgeHandle } from "."

const STEP = 0.1, MIN = 0.5, MAX = 2 // (host/ui_scale.h: MIN_CHOSEN, MAX_CHOSEN)

/**
 * Ctrl and the mouse wheel size the interface, as in a browser: the `ui_scale` setting, which the
 * engine multiplies into the page's scale over the window's own fit (host/ui_scale.h). A burst of
 * notches lands as one change, read against the setting as it is then (the slider may have moved it).
 */
export function Zoom(opts: { bridge: BridgeHandle }) {
    let notches = 0
    let timer: ReturnType<typeof setTimeout> | undefined
    const apply = async () => {
        const steps = notches
        notches = 0
        const now = (await opts.bridge.send("settings.get", {})).ui_scale
        if (typeof now !== "number") throw new Error("settings.get has no ui_scale")
        const next = Math.min(MAX, Math.max(MIN, Math.round((now + steps * STEP) * 20) / 20))
        if (next !== now) await opts.bridge.send("settings.set", { key: "ui_scale", value: next })
    }
    window.addEventListener("wheel", (e) => {
        if (!e.ctrlKey) return
        e.preventDefault()
        notches += e.deltaY < 0 ? 1 : -1
        clearTimeout(timer)
        timer = setTimeout(() => void apply().catch((error) => console.error("ui_scale", error)), 80)
    }, { passive: false })
}
