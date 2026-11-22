import { raster, type Raster, type Rgba } from "../../image"

/** A cube's three faces as it's seen from above, front-left: the top, the left side, the right. */
export type CubeFaces = { readonly top: Raster; readonly left: Raster; readonly right: Raster }

/** How much light each face gets: one light, top-left. */
const LIGHT = { top: 1, left: 0.82, right: 0.62 }

/**
 * iso — a block drawn as players see it in the inventory: a 2:1 isometric cube, `k` screen
 * pixels to a texel's width. Faces are sampled, so nothing's lost at any size.
 */
export const iso = {
    cube(faces: CubeFaces, k: number): Raster {
        const n = faces.top.width
        const out = raster.blank(2 * n * k, 2 * n * k)
        // screen x = (X - Z)k, y = (X + Z)k/2 - Yk, for the cube [0, n]³, shifted onto the picture
        for (let py = 0; py < out.height; py++)
            for (let px = 0; px < out.width; px++) {
                const sx = (px + 0.5) / k - n, sy = (py + 0.5) / k - n
                const c = sample(faces, n, sx, sy)
                if (c) raster.set(out, px, py, c)
            }
        return out
    },
}

/** The colour at screen point (sx, sy) in texel units, or null off the cube. */
function sample(f: CubeFaces, n: number, sx: number, sy: number): Rgba | null {
    // the top, Y = n: X - Z = sx, X + Z = 2(sy + n)
    const tx = (sx + 2 * (sy + n)) / 2, tz = (2 * (sy + n) - sx) / 2
    if (tx >= 0 && tx < n && tz >= 0 && tz < n) return lit(raster.get(f.top, Math.floor(tx), Math.floor(tz)), LIGHT.top)
    // the left side, Z = n: X = sx + n, Y = (X + n)/2 - sy
    const lx = sx + n, ly = (lx + n) / 2 - sy
    if (lx >= 0 && lx < n && ly > 0 && ly <= n) return lit(raster.get(f.left, Math.floor(lx), Math.floor(n - ly)), LIGHT.left)
    // the right side, X = n: Z = n - sx, Y = (n + Z)/2 - sy; u runs left to right, so n - Z
    const rz = n - sx, ry = (n + rz) / 2 - sy
    if (rz >= 0 && rz < n && ry > 0 && ry <= n) return lit(raster.get(f.right, Math.floor(n - rz), Math.floor(n - ry)), LIGHT.right)
    return null
}

function lit(c: Rgba, light: number): Rgba | null {
    if (c[3] === 0) return null
    return [Math.round(c[0] * light), Math.round(c[1] * light), Math.round(c[2] * light), c[3]]
}
