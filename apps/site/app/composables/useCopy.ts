/**
 * Copy text to the clipboard, with a label that says how it went for a moment.
 * A browser that refuses the clipboard is asked to select the text instead.
 */
export function useCopy(idle = "copy") {
    const label = ref(idle)
    let timer: ReturnType<typeof setTimeout> | undefined

    async function copy(text: string) {
        try {
            await navigator.clipboard.writeText(text)
            label.value = "copied"
        } catch {
            label.value = "select it"
        }
        clearTimeout(timer)
        timer = setTimeout(() => (label.value = idle), 1600)
    }

    return { label: label, copy: copy }
}
