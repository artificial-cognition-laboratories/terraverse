import type { Art } from "./sky"

/** Art as an image URL, one canvas pixel per art pixel; CSS scales it up crisp. Browser only. */
export function paint(art: Art): string {
    const canvas = document.createElement("canvas")
    canvas.width = art.width
    canvas.height = art.height
    const context = canvas.getContext("2d")
    if (!context) throw new Error("paint: no 2D canvas")
    for (const p of art.pixels) {
        context.fillStyle = p.fill
        context.fillRect(p.x, p.y, p.w, p.h)
    }
    return canvas.toDataURL("image/png")
}
