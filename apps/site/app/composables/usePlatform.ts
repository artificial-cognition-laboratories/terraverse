import type { PlatformId } from "~/config/downloads"

/**
 * The visitor's OS, or null until it's known: the server has no visitor, so
 * the prerendered page must not guess. Read once on mount.
 */
export function usePlatform() {
    const detected = ref<PlatformId | null>(null)

    onMounted(() => {
        const nav = navigator as Navigator & { userAgentData?: { platform: string } }
        const name = (nav.userAgentData?.platform ?? navigator.userAgent).toLowerCase()
        // A phone can't run it, so it gets no guess: Android reports Linux
        if (name.includes("android")) return
        if (name.includes("win")) detected.value = "windows"
        else if (name.includes("mac") || name.includes("iphone") || name.includes("ipad")) detected.value = "macos"
        else if (name.includes("linux") || name.includes("x11")) detected.value = "linux"
    })

    return { detected }
}
