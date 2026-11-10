import { onBeforeUnmount, onMounted } from "vue"
import type { Ref } from "vue"

/** Closes a popover on a click outside `root` (and `popup`, when it's teleported elsewhere) or on Escape. */
export function useDismiss(root: Ref<HTMLElement | null>, open: Ref<boolean>, popup?: Ref<HTMLElement | null>) {
    function onPointerDown(event: PointerEvent) {
        const target = event.target
        if (!open.value || !(target instanceof Node)) return
        if (root.value?.contains(target) || popup?.value?.contains(target)) return
        open.value = false
    }
    function onKeyDown(event: KeyboardEvent) {
        if (event.key === "Escape" && open.value) open.value = false
    }
    onMounted(() => {
        document.addEventListener("pointerdown", onPointerDown)
        document.addEventListener("keydown", onKeyDown)
    })
    onBeforeUnmount(() => {
        document.removeEventListener("pointerdown", onPointerDown)
        document.removeEventListener("keydown", onKeyDown)
    })
}
