import type { SceneRect } from "../bridge/index.ts"

/**
 * Where a native scene goes: the element's box in the page's pixels, and the scale the page is
 * laid out at, so the engine turns it into the window's pixels by the page's own measure, never
 * its idea of it (the two part while a new scale is on its way to Chromium). Null while it has no size.
 */
export function sceneRect(el: HTMLElement | null): SceneRect | null {
    const r = el?.getBoundingClientRect()
    return r && r.width > 0 ? { x: r.left, y: r.top, width: r.width, height: r.height, scale: window.devicePixelRatio } : null
}
