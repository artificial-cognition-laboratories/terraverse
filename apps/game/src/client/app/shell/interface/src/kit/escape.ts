import { onBeforeUnmount, onMounted } from "vue"

/**
 * Esc leaves the screen that calls this, while it's shown. Whatever handles Esc nearer the key
 * (an open dropdown, a key being recorded) prevents it, and then it isn't taken here. The engine
 * hands Esc to the page in the menu (host/shell.cpp), and in game while an input has the keyboard.
 */
export function onEscape(leave: () => void): void {
    const key = (e: KeyboardEvent) => {
        if (e.key !== "Escape" || e.defaultPrevented) return
        e.preventDefault()
        leave()
    }
    onMounted(() => window.addEventListener("keydown", key))
    onBeforeUnmount(() => window.removeEventListener("keydown", key))
}
