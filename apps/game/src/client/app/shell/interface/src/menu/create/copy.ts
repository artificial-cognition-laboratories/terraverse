import { ref } from "vue"

/** Copy text to the clipboard, with a short-lived "copied" / error state for the button. */
export function Copier() {
    const state = ref<"idle" | "copied" | "failed">("idle")
    let timer = 0
    return {
        state: state,
        async copy(text: string) {
            clearTimeout(timer)
            try {
                await navigator.clipboard.writeText(text)
                state.value = "copied"
            } catch (err) {
                console.error(err)
                state.value = "failed"
            }
            timer = window.setTimeout(() => (state.value = "idle"), 1600)
        },
    }
}
