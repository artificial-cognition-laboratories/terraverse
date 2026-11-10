/**
 * Is the viewport narrow enough that a page's left sidebar should move into
 * the topnav drawer?
 *
 * Every sidebar on the site used to collapse to `width: 100%; position:
 * static` below tablet, which put a full-width wall of navigation above the
 * content on every page — a phone visitor scrolled past the whole docs tree
 * before reaching a word of the article. The nav belongs in the drawer the
 * site already has; only the CONTENT should lead the page.
 *
 * Consumers pair this with `<Teleport to="#mobile-page-nav" :disabled="!isMobileNav">`,
 * so one markup block serves both layouts: rendered in place on desktop,
 * relocated into the drawer on mobile. No duplicated sidebar, no second
 * component to keep in step.
 *
 * ── Why not a CSS media query ───────────────────────────────────────────────
 *
 * Teleporting is a DOM move, not a style, so the decision has to exist in
 * JavaScript. The breakpoint is duplicated from `$bp-tablet` in
 * variables.scss and must stay equal to it — if they disagree, a width exists
 * where the sidebar is teleported into a drawer that is still hidden, and the
 * navigation vanishes entirely.
 */

/** Must equal `$bp-tablet` in app/assets/styles/variables.scss. */
const TABLET_BREAKPOINT = 900

export function useMobileNav() {
    // Starts false so SSR and the client's first render agree: the server has
    // no viewport, and guessing "mobile" there would make every desktop load
    // hydrate with the sidebar in the wrong place.
    const isMobileNav = ref(false)

    let query: MediaQueryList | null = null
    const sync = (event: MediaQueryList | MediaQueryListEvent) => {
        isMobileNav.value = event.matches
    }

    onMounted(() => {
        query = window.matchMedia(`(max-width: ${TABLET_BREAKPOINT}px)`)
        sync(query)
        query.addEventListener("change", sync)
    })

    onBeforeUnmount(() => query?.removeEventListener("change", sync))

    return { isMobileNav }
}
