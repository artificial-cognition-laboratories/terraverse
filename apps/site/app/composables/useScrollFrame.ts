/**
 * Run `frame` once per animation frame while the page scrolls or resizes, with the
 * scroll position. Nothing runs for a visitor who asks for reduced motion: the
 * scene holds still where it was first drawn.
 */
export function useScrollFrame(frame: (scroll: number) => void) {
    let queued = 0

    function schedule() {
        if (queued) return
        queued = requestAnimationFrame(() => {
            queued = 0
            frame(window.scrollY)
        })
    }

    onMounted(() => {
        if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) return
        window.addEventListener("scroll", schedule, { passive: true })
        window.addEventListener("resize", schedule)
        schedule()
    })

    onBeforeUnmount(() => {
        window.removeEventListener("scroll", schedule)
        window.removeEventListener("resize", schedule)
        cancelAnimationFrame(queued)
    })
}
