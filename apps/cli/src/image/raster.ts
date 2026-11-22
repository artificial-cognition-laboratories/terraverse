/** A picture in memory: RGBA, 8 bits each, rows top to bottom. */
export type Raster = { readonly width: number; readonly height: number; readonly data: Uint8Array }

/** An RGBA colour, each 0–255. */
export type Rgba = readonly [r: number, g: number, b: number, a: number]

/** raster — making pictures and reading their pixels. */
export const raster = {
    /** A clear picture this size. */
    blank(width: number, height: number): Raster {
        return { width: width, height: height, data: new Uint8Array(width * height * 4) }
    },

    get(r: Raster, x: number, y: number): Rgba {
        const i = (y * r.width + x) * 4
        return [r.data[i]!, r.data[i + 1]!, r.data[i + 2]!, r.data[i + 3]!]
    },

    /** Paint c over what's there, by its alpha. Off the picture is nothing. */
    put(r: Raster, x: number, y: number, c: Rgba): void {
        if (x < 0 || y < 0 || x >= r.width || y >= r.height) return
        const i = (y * r.width + x) * 4
        const a = c[3] / 255
        const under = r.data[i + 3]! / 255
        const out = a + under * (1 - a)
        if (out === 0) return
        for (let k = 0; k < 3; k++)
            r.data[i + k] = Math.round((c[k]! * a + r.data[i + k]! * under * (1 - a)) / out)
        r.data[i + 3] = Math.round(out * 255)
    },

    /** Replace a pixel outright, alpha and all. */
    set(r: Raster, x: number, y: number, c: Rgba): void {
        const i = (y * r.width + x) * 4
        r.data[i] = c[0]; r.data[i + 1] = c[1]; r.data[i + 2] = c[2]; r.data[i + 3] = c[3]
    },

    /** src drawn onto dst at (x, y), each pixel a scale × scale square. */
    blit(dst: Raster, src: Raster, x: number, y: number, scale = 1): void {
        for (let sy = 0; sy < src.height; sy++)
            for (let sx = 0; sx < src.width; sx++) {
                const c = raster.get(src, sx, sy)
                if (c[3] === 0) continue
                for (let dy = 0; dy < scale; dy++)
                    for (let dx = 0; dx < scale; dx++) raster.put(dst, x + sx * scale + dx, y + sy * scale + dy, c)
            }
    },

    /** A rectangle filled with c. */
    rect(r: Raster, x: number, y: number, w: number, h: number, c: Rgba): void {
        for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) raster.put(r, x + i, y + j, c)
    },
}
