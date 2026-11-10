import { areaOf, areas, type DocsNavItem, type DocsSection } from "~/config/docs-nav"

/**
 * Docs and wiki navigation state, derived from the route: the one place the
 * area and active section are resolved. The subnav, the sidebar and the drawer all render from
 * this; none derives anything itself.
 */
export function useDocsNav() {
    const route = useRoute()
    const router = useRouter()

    const area = computed(() => areaOf(route.path))
    const sections = computed(() => areas[area.value])

    // The section whose nav holds the longest match for the route
    const routeSection = computed<DocsSection | undefined>(() => {
        let best: DocsSection | undefined
        let bestLen = 0
        for (const section of sections.value) {
            const len = Math.max(
                matchLength(section.path, route.path),
                ...section.groups.map(g => longestMatch(g.items, route.path)),
            )
            if (len > bestLen) {
                bestLen = len
                best = section
            }
        }
        return best
    })

    // Shared and stable: never snaps back to the first tab mid-navigation
    const activeSection = useState<DocsSection | undefined>(
        "docs-active-section",
        () => routeSection.value ?? sections.value[0],
    )

    watch(routeSection, resolved => {
        if (resolved) activeSection.value = resolved
    })

    function select(section: DocsSection) {
        activeSection.value = section
        router.push(section.path)
    }

    return { area, sections, activeSection, select }
}

// An exact match beats any prefix; an area's root is everyone's prefix, so it
// only counts exactly.
function matchLength(path: string, route: string): number {
    if (route === path) return path.length + 1000
    if (path !== "/docs" && path !== "/wiki" && route.startsWith(path + "/")) return path.length
    return 0
}

function longestMatch(items: DocsNavItem[], route: string): number {
    let best = 0
    for (const item of items) {
        if (item.path) best = Math.max(best, matchLength(item.path, route))
        if (item.items) best = Math.max(best, longestMatch(item.items, route))
    }
    return best
}
