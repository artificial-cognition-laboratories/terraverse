/**
 * One draw loop for every Live chart: a chart marks itself stale, and the
 * stale ones are drawn together in one animation frame. Each chart asked for
 * its own frame before, every message, so the panel's drawing landed in the
 * game's frames in pieces.
 *
 * Every second animation frame (the page's frames are the game's): each
 * picture held for exactly two of the game's frames. Capped by time instead
 * (30 a second against the game's 70), a picture was held two frames, then
 * three: the charts slid in uneven steps, a judder however cheap the drawing.
 */
const EVERY = 2

const stale = new Set<() => void>()
let pending: number | null = null
let tick = 0

function run() {
    pending = null
    if (++tick % EVERY !== 0) {
        pending = requestAnimationFrame(run) // not this frame: the next
        return
    }
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
