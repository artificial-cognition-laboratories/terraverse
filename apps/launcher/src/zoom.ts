/**
 * The launcher's scale: the page zoomed as a whole (text, glass, corners alike), so it sits at the
 * game's size in a small window. Ctrl+scroll and Ctrl +/−/0 change it, as in a browser: the
 * webview doesn't take Ctrl+scroll on every platform (not WebKitGTK), so the page listens for it.
 * `apply` does the zooming (the webview's own zoom in the app, CSS in a browser); the choice is
 * kept between runs.
 */
const KEY = "launcher.zoom"
const DEFAULT = 0.85
const MIN = 0.6
const MAX = 1.5
const STEP = 0.05

export function Zoom(opts: { apply: (scale: number) => void; storage: Storage }) {
    const kept = Number(opts.storage.getItem(KEY))
    let scale = kept >= MIN && kept <= MAX ? kept : DEFAULT

    function set(next: number) {
        scale = Math.round(Math.min(MAX, Math.max(MIN, next)) * 100) / 100
        opts.apply(scale)
        opts.storage.setItem(KEY, String(scale))
    }

    // + and = share a key; 0 is back to where it started
    const KEYS: Record<string, () => void> = {
        "=": () => set(scale + STEP),
        "+": () => set(scale + STEP),
        "-": () => set(scale - STEP),
        "0": () => set(DEFAULT),
    }

    function start() {
        set(scale)
        window.addEventListener("wheel", event => {
            if (!event.ctrlKey) return
            event.preventDefault()
            set(scale + (event.deltaY < 0 ? STEP : -STEP))
        }, { passive: false })
        window.addEventListener("keydown", event => {
            const action = (event.ctrlKey || event.metaKey) ? KEYS[event.key] : undefined
            if (!action) return
            event.preventDefault()
            action()
        })
    }

    return { start: start }
}
export type Zoom = ReturnType<typeof Zoom>
