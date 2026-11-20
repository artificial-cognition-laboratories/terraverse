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
}
