/**
 * One draw loop for every Live chart: a chart marks itself stale, and the
 * stale ones are drawn together, at most 30 times a second, in one animation
 * frame. Each chart asked for its own frame before, every message, so the
 * panel's drawing landed in the game's frames in pieces.
 */
const MIN_GAP_MS = 1000 / 30

const stale = new Set<() => void>()
let pending: number | null = null
let last = 0

function run(now: number) {
    pending = null
    if (now - last < MIN_GAP_MS) {
        pending = requestAnimationFrame(run) // too soon: the next frame
        return
    }
    last = now
    const drawing = [...stale]
    stale.clear()
    for (const draw of drawing) draw()
}

/** Draws `draw` with the rest, soon. */
export function markStale(draw: () => void) {
    stale.add(draw)
    if (pending === null) pending = requestAnimationFrame(run)
}

/** A chart going: never drawn again. */
export function forget(draw: () => void) {
    stale.delete(draw)
}
