import type { BridgeHandle } from "."

function editable(el: EventTarget | null) {
    return el instanceof HTMLElement && (el.isContentEditable || el.matches("input, textarea, select"))
}

/** The page holds the keyboard exactly while an editable element has focus. */
export function Keyboard(opts: { bridge: BridgeHandle }) {
    let held = false
    const sync = () => {
        const want = editable(document.activeElement)
        if (want === held) return
        held = want
        opts.bridge.send("capture", { keyboard: want })
    }
    document.addEventListener("focusin", sync)
    document.addEventListener("focusout", () => queueMicrotask(sync))
    // an input taken out of the page while it has focus (the inventory's search, as its tab changes)
    // fires no focusout: without this the page would keep every key, Esc too, with nothing to type in
    let queued = false
    new MutationObserver(() => {
        if (queued) return
        queued = true
        queueMicrotask(() => { queued = false; sync() })
    }).observe(document.body, { childList: true, subtree: true })
}
