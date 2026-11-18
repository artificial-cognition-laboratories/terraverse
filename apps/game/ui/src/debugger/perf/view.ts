/**
 * Ported from Axon Fleet's flame graph (arclabs, apps/fleet/webview/src/components/flamegraph/view.ts).
 *
 * The viewport: which slice of time the graph is showing.
 *
 * This is the one piece of state the old renderer did not own, and every
 * live-mode bug came from that. `flame-chart-js` kept the viewport inside
 * its render engine and reset it on every `setData()`, so a live graph
 * spent each frame having its viewport clobbered and restored.
 *
 * **Following is derived, not declared.** There is no follow flag and no
 * follow button: a view is following when its right edge is against the
 * live end, which is a fact about the window rather than a mode beside
 * it. That is what makes the behaviour explainable in one sentence —
 * *the graph advances when you are looking at the end of it* — and it is
 * why nothing in the UI has to display the state. The window's position
 * already shows it, in the minimap.
 *
 * Two consequences worth stating, because they are the difference between
 * this feeling automatic and feeling flaky:
 *
 * **Follow is sticky once engaged.** Reaching the edge enters it;
 * only a deliberate move away leaves it. A naive "is the window within
 * slack of the end" test read live every frame fails badly at high zoom,
 * where the slack is a tiny slice of time and one burst of events
 * outruns the window — silently dropping the user out of follow while
 * they sit still. Data arriving must never change the mode.
 *
 * **Zoomed fully out is a distinct state**, not merely a wide follow.
 * The window is pinned to the whole session and grows with it. Zooming
 * out to the end lands here; it is the "watch the entire flow" case, and
 * it is deliberately the rarer one to fall into.
 */

/** Never zoom past this — below it, bars are thinner than a pixel anyway. */
const MIN_SPAN_MS = 0.02 // a frame's zones run to microseconds

/**
 * How close to the live end still counts as being against it.
 *
 * Proportional to the visible span rather than a fixed number of
 * milliseconds: 500ms is "basically at the end" of a five-minute view and
 * "miles away" from a 20ms one, so a fixed slack makes follow easy to
 * enter when zoomed out and impossible when zoomed in. Two percent of the
 * window means the gesture feels the same at every magnification.
 */
const EDGE_FRACTION = 0.02

export interface Window {
    startMs: number
    endMs: number
}

export interface ViewOpts {
    /** Total extent of the data, re-read on every frame. */
    bounds: () => Window
    /**
     * The window a fresh view opens at, in ms. The pane opens following at
     * this width rather than fully zoomed out: a session that has been
     * running an hour would otherwise open as an unreadable smear, and the
     * question a user has on opening the panel is almost always "what is it
     * doing now".
     *
     * Clamped to the session, so a short run just shows all of itself.
     */
    initialSpanMs?: number
    /** The widest it zooms out to: a view over more than this is never drawn. */
    maxSpanMs?: number
}

export function View(opts: ViewOpts) {
    /**
     * The held window, or null while pinned to the full extent.
     *
     * Null means "zoomed fully out" — the window IS the bounds and grows
     * with them. Representing that as an absent window rather than a
     * remembered one makes the invalid state (pinned to full, but holding
     * a stale window that disagrees with the data) unrepresentable.
     */
    let held: Window | null = null

    /**
     * Whether the held window tracks the live end.
     *
     * Set when a gesture leaves the window against the edge, cleared when
     * one moves it away. Never touched by data arrival — see the note on
     * stickiness above.
     */
    let tracking = true

    /**
     * Whether the opening window has been established yet.
     *
     * Seeding is lazy because a pane mounts before the session has any
     * extent — at construction the bounds are a single millisecond, and a
     * one-minute window clamped to that is just "everything". So the first
     * read that finds real extent adopts the default span.
     *
     * It must also be considered done the moment the user moves the view,
     * or a gesture made before the session grew past a minute would be
     * silently overwritten by the seed on the next frame.
     */
    let seeded = false

    const clamp = (window: Window): Window => {
        const limit = opts.bounds()
        const full = limit.endMs - limit.startMs
        const widest = Math.min(Math.max(full, MIN_SPAN_MS), opts.maxSpanMs ?? Infinity)
        const span = Math.min(Math.max(window.endMs - window.startMs, MIN_SPAN_MS), widest)
        // Slide rather than squash: a window pushed past an edge keeps its
        // width and stops, the way every map and timeline behaves.
        const start = Math.min(Math.max(window.startMs, limit.startMs), Math.max(limit.startMs, limit.endMs - span))
        return { startMs: start, endMs: start + span }
    }

    /** Is this window's right edge against the live end? */
    const atEdge = (window: Window): boolean => {
        const limit = opts.bounds()
        const slack = Math.max((window.endMs - window.startMs) * EDGE_FRACTION, MIN_SPAN_MS)
        return window.endMs >= limit.endMs - slack
    }

    /**
     * Take hold of a window, deciding follow from where it landed.
     *
     * The single place `tracking` is written, so "follow means the right
     * edge is at the live end" cannot drift between gestures.
     *
     * `engage` is whether reaching the edge is allowed to START following.
     * It is false for zooming out from a detached view: widening a window
     * eventually pushes its right edge into the slack no matter where the
     * user is looking, so an unconditional test would silently snap them
     * into follow mid-session — a mode change nobody asked for. Zooming
     * out only follows when it reaches the full extent, which is the
     * explicit "show me everything" gesture.
     */
    const hold = (window: Window, engage = true): void => {
        // Any deliberate move settles the opening window — see `seeded`.
        seeded = true
        const limit = opts.bounds()
        const next = clamp(window)
        // Zoomed back out to everything is not a window being held — it is
        // the full-extent state, which follows by definition.
        if (next.endMs - next.startMs >= limit.endMs - limit.startMs) {
            held = null
            tracking = true
            return
        }
        held = next
        tracking = atEdge(next) && (engage || tracking)
    }

    return {
        /** The window to draw right now. */
        get current(): Window {
            const limit = opts.bounds()

            // Seed the opening window once the session has real extent.
            if (!seeded) {
                const span = opts.initialSpanMs
                if (span !== undefined && limit.endMs - limit.startMs > span) {
                    seeded = true
                    held = { startMs: limit.endMs - span, endMs: limit.endMs }
                    tracking = true
                }
            }

            if (!held) return limit
            // Following: the window keeps its width and rides the live end.
            // Recomputed on read rather than pushed on data arrival, so a
            // frame can never render a window one tick behind the data.
            if (tracking) {
                const span = held.endMs - held.startMs
                return clamp({ startMs: limit.endMs - span, endMs: limit.endMs })
            }
            // Re-clamped on read: the data grows underneath a held window,
            // and a window legal a second ago may now sit inside a larger
            // extent.
            return clamp(held)
        },

        /** True when the view advances with the live end. */
        get following(): boolean {
            return tracking
        },

        /** True when pinned to the whole session — follow at max zoom. */
        get full(): boolean {
            return held === null
        },

        /** Keep a detached viewport on the same instants after ring eviction. */
        rebase(deltaMs: number): void {
            if (!held || tracking) return
            held = clamp({ startMs: held.startMs - deltaMs, endMs: held.endMs - deltaMs })
        },

        /** Pan by a pixel delta, given the canvas width. */
        pan(deltaPx: number, widthPx: number): void {
            if (widthPx <= 0) return
            const window = this.current
            const perPx = (window.endMs - window.startMs) / widthPx
            const shift = deltaPx * perPx
            hold({ startMs: window.startMs + shift, endMs: window.endMs + shift })
        },

        /**
         * Zoom by `scale` about a normalized anchor (0 = left, 1 = right).
         *
         * Anchored zoom is what makes the grid feel pinned: the time under
         * the cursor is the fixed point, so lines and bars scale together
         * about it rather than the whole picture sliding.
         */
        zoom(scale: number, anchor: number): void {
            const window = this.current
            const span = window.endMs - window.startMs
            const focus = window.startMs + span * anchor
            const next = span * scale

            // Zooming IN hard against the right edge converges on the live
            // end, rather than on wherever the edge happens to be now.
            //
            // Plain anchored zoom holds `endMs` fixed while the slack
            // shrinks with the span — so a view sitting 2s short of live
            // keeps that 2s forever and gets RELATIVELY further from
            // following the more you zoom. Converging on the live end
            // instead means "zoom in at the right edge" reliably arrives at
            // follow, which is the gesture people actually reach for.
            //
            // ONLY when the anchor is hard right (the canvas snaps to
            // exactly 1 near the edge). Everywhere else anchored zoom is
            // correct and must not drag the window toward live: zooming in
            // on the left of the canvas means "show me this older work
            // closer", and pinning that to the live end would both move the
            // thing being pointed at and make follow impossible to leave by
            // zooming — which is exactly how it felt when this branch was
            // written to catch every anchor.
            const limit = opts.bounds()
            if (anchor === 1 && next <= span) {
                // Close the gap FASTER than the window shrinks.
                //
                // Easing at the same rate (gap *= next/span) keeps the gap
                // a constant fraction of the view, so it converges on the
                // edge without ever reaching it — 26 notches and still
                // detached. Squaring the ratio makes each notch also cut
                // the gap's share of the window, so a few notches arrive
                // while the motion still reads as a smooth approach rather
                // than a snap.
                const ease = (next / span) ** 2
                const gap = Math.max(0, limit.endMs - window.endMs) * ease
                const end = limit.endMs - gap
                hold({ startMs: end - next, endMs: end })
                return
            }

            // Otherwise the window lands where the anchor puts it, and
            // follow is decided by where it LANDED.
            //
            // `engage` is withheld when zooming OUT from a detached view:
            // widening a window eventually pushes its right edge into the
            // slack wherever the user is looking, and snapping them into
            // follow mid-session is a mode change nobody asked for.
            hold(
                { startMs: focus - next * anchor, endMs: focus + next * (1 - anchor) },
                next <= span,
            )
        },

        /** Frame an explicit time range — the minimap dragging the view. */
        show(startMs: number, endMs: number): void {
            hold({ startMs, endMs })
        },

        /** Jump to the live end, keeping the current span. */
        follow(): void {
            const limit = opts.bounds()
            const span = this.current.endMs - this.current.startMs
            hold({ startMs: limit.endMs - span, endMs: limit.endMs })
        },
    }
}

export type ViewHandle = ReturnType<typeof View>
