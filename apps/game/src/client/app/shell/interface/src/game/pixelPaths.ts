/**
 * Pixel art as SVG paths: one path a colour, each row's runs of it a
 * rectangle. A rect a pixel was thousands of elements on Play's world list
 * (a globe and chips for every world), its mount a long task the menu hitched
 * on. Kept by the art and palette: the same icon's drawn once.
 */
export type Drawn = { width: number; paths: { fill: string; d: string }[] }

const known = new WeakMap<readonly string[], Map<string, Drawn>>()

export function drawn(rows: readonly string[], palette: Record<string, string>): Drawn {
    let byPalette = known.get(rows)
    if (!byPalette) known.set(rows, (byPalette = new Map()))
    const key = JSON.stringify(palette)
    let out = byPalette.get(key)
    if (!out) byPalette.set(key, (out = draw(rows, palette)))
    return out
}

function draw(rows: readonly string[], palette: Record<string, string>): Drawn {
    const paths = new Map<string, string>() // a fill's path
    rows.forEach((row, y) => {
        let x = 0
        while (x < row.length) {
            const key = row[x]!
            let end = x + 1
            while (end < row.length && row[end] === key) end++
            const fill = palette[key]
            if (key !== "." && fill) paths.set(fill, `${paths.get(fill) ?? ""}M${x} ${y}h${end - x}v1h${x - end}z`)
            x = end
        }
    })
    return {
        width: Math.max(...rows.map((r) => r.length)),
        paths: [...paths].map(([fill, d]) => ({ fill: fill, d: d })),
    }
}
