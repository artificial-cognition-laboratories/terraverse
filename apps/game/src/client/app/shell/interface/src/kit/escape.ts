import { onBeforeUnmount, onMounted } from "vue"

/**
 * Esc leaves the screen that calls this, while it's shown. Whatever handles Esc nearer the key
 * (an open dropdown, a key being recorded) prevents it, and then it isn't taken here. The engine
 * hands Esc to the page in the menu (host/shell.cpp), and in game while an input has the keyboard.
 * `over`: it's shown over a screen that leaves on Esc too (a popover), so it takes the key first.
 */
export function onEscape(leave: () => void, opts: { over?: boolean } = {}): void {
    const key = (e: KeyboardEvent) => {
        if (e.key !== "Escape" || e.defaultPrevented) return
        e.preventDefault()
        leave()
    }
    const capture = opts.over === true
    onMounted(() => window.addEventListener("keydown", key, { capture: capture }))
    onBeforeUnmount(() => window.removeEventListener("keydown", key, { capture: capture }))
}
