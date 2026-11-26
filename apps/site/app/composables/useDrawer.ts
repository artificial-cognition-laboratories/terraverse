/**
 * The topnav's drawer on a phone, open or not: one state, so what lives outside the topnav (the
 * docs' where-you-are bar) can open it too. Any navigation closes it.
 */
export function useDrawer() {
    const open = useState("drawer-open", () => false)
    return {
        open: open,
        toggle: () => (open.value = !open.value),
        show: () => (open.value = true),
    }
}
