import { onBeforeUnmount, ref, watch, type Ref } from "vue"
import type { BridgeHandle, PlanetariumBody, PlanetariumFlight, PlanetariumLayer, PlanetariumMarker, PlanetariumPlace, PlanetariumViewport } from "../bridge"

/**
 * The native Planetarium (src/shell/planetarium.h) under a part of the page:
 * shown while `active`, drawn over the whole window or where an element is (followed as it moves or
 * resizes), the body swapped in when it changes (the old one stays on
 * screen until the new one is ready), markers kept up to date. Hidden when
 * inactive or unmounted. `problem` holds the last refusal.
 */
export function usePlanetarium(opts: {
    bridge: BridgeHandle
    /** Where it's drawn: the whole window, under the page (the menu's universe), or where an element is. */
    place: "whole" | Ref<HTMLElement | null>
    active: Ref<boolean>
    body: Ref<PlanetariumBody>
    markers?: Ref<PlanetariumMarker[]>
    /** What the page covers of the window's sides (page pixels: Dock's `covers`): the planet is framed in the rest. */
    frame?: Ref<{ left: number; right: number }>
    /** Where the camera starts, the first time it's shown. */
    start?: () => (PlanetariumPlace & { altitude?: number }) | null
}) {
    const problem = ref<string | null>(null)
    let shown = false
    let started = false
    const fail = (what: string) => (error: unknown) => {
        problem.value = error instanceof Error ? error.message : String(error)
        console.error(what, error)
    }
    const WHOLE: PlanetariumViewport = { whole: true }
    const viewport = (): PlanetariumViewport => {
        if (opts.place === "whole") return WHOLE
        const rect = opts.place.value?.getBoundingClientRect()
        return rect && rect.width > 0 && rect.height > 0 ? { x: rect.left, y: rect.top, width: rect.width, height: rect.height } : WHOLE
    }
    function show() {
        if (!opts.active.value || (opts.place !== "whole" && !opts.place.value)) return hide()
        const focus = started ? undefined : opts.start?.() ?? undefined
        started ||= !!focus
        shown = true
        problem.value = null
        void opts.bridge.send("planetarium.show", {
            body: opts.body.value, viewport: viewport(), markers: opts.markers?.value ?? [], ...(focus ? { focus } : {}),
        }).catch(fail("planetarium.show"))
        framed()
    }
    function hide() {
        if (!shown) return
        shown = false
        void opts.bridge.send("planetarium.hide", {}).catch(fail("planetarium.hide"))
    }
    /** Over a place on the body, or to another body of its system (by id). */
    function focus(place: (PlanetariumPlace & { altitude?: number }) | { body: string }) {
        if (shown) void opts.bridge.send("planetarium.focus", place).catch(fail("planetarium.focus"))
    }
    /** Eases the camera there over `seconds`: to a body (by id), over a place, at an altitude (blocks). */
    function fly(flight: PlanetariumFlight) {
        if (shown) void opts.bridge.send("planetarium.fly", flight).catch(fail("planetarium.fly"))
    }
    /** What's over the land: an overlay painted natively, an image the page painted (the weather), the body cut open, or nothing. */
    function layer(what: Exclude<PlanetariumLayer, { width: number }> | ImageData | null) {
        if (!shown) return
        const request: PlanetariumLayer = what === null ? { none: true }
            : what instanceof ImageData ? { width: what.width, height: what.height, rgba: base64(what.data) } : what
        void opts.bridge.send("planetarium.layer", request).catch(fail("planetarium.layer"))
    }
    /** The host's clock: stopped or running, and the date (0..1 through the year). */
    function time(clock: { running?: boolean; date?: number }) {
        if (shown) void opts.bridge.send("planetarium.time", clock).catch(fail("planetarium.time"))
    }
    /** Whether the atlas's seams are drawn. */
    function seams(shown_: boolean) {
        if (shown) void opts.bridge.send("planetarium.seams", { shown: shown_ }).catch(fail("planetarium.seams"))
    }
    const follow = () => { if (shown) void opts.bridge.send("planetarium.viewport", viewport()).catch(fail("planetarium.viewport")) }
    const framed = () => {
        if (shown && opts.frame) void opts.bridge.send("planetarium.frame", opts.frame.value).catch(fail("planetarium.frame"))
    }
    if (opts.frame) watch(opts.frame, framed, { deep: true })
    const resized = new ResizeObserver(follow)
    if (opts.place === "whole") show()
    else
        watch(opts.place, (el, was) => {
            if (was) resized.unobserve(was)
            if (el) resized.observe(el)
            show()
        }, { immediate: true })
    window.addEventListener("resize", follow)
    watch([opts.active, () => JSON.stringify(opts.body.value)], show)
    if (opts.markers)
        watch(opts.markers, (markers) => {
            if (shown) void opts.bridge.send("planetarium.markers", { markers }).catch(fail("planetarium.markers"))
        }, { deep: true })
    onBeforeUnmount(() => {
        resized.disconnect()
        window.removeEventListener("resize", follow)
        hide()
    })
    /** Asked for again: a new host (the menu back after a world wasn't entered) has none of what it was told. */
    function again() {
        shown = false
        show()
    }
    return { problem, focus, fly, layer, time, seams, again }
}

// bytes as base64, in chunks (a whole image at once overflows the call stack)
function base64(bytes: Uint8ClampedArray) {
    let text = ""
    for (let i = 0; i < bytes.length; i += 0x8000) text += String.fromCharCode(...bytes.subarray(i, i + 0x8000))
    return btoa(text)
}
