/**
 * A wheel's turn: with Shift held most browsers report it sideways (deltaX),
 * so whichever way it came.
 */
export function wheelDelta(event: WheelEvent) {
    return Math.abs(event.deltaY) >= Math.abs(event.deltaX) ? event.deltaY : event.deltaX
}
