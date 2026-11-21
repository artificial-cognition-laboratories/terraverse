import { Live, type LiveHandle, type RendererHandle, type Step } from "@arcforge/arcline"
import { errOf, type TerraError } from "../../errors"
import { Steps } from "./steps"

/** What a surface's view is drawn from: its steps, then its result or how it failed. */
export type SurfaceState<R> = { steps: Step[]; result?: R; failure?: { error: TerraError } }

/**
 * A failure a surface already drew, under its steps. The boundary exits 1 for it and
 * prints nothing more: printing it again would put the error twice, the second time
 * without the steps that locate it.
 */
export class Shown extends Error {
    constructor(readonly error: TerraError) {
        super(error.message)
    }
}

/**
 * Surface — one command's live view: its steps counting while the work runs, then the
 * result, or the failure under the steps that got there. In a pipe it draws only the last
 * frame; for --json, nothing.
 */
export function Surface<R>(opts: {
    renderer: RendererHandle
    machine: boolean
    labels: readonly string[]
    view: (r: RendererHandle, state: SurfaceState<R>, frame: string) => string
}) {
    const steps = Steps({ labels: opts.labels, onChange: current => live?.set({ steps: current }) })
    const live: LiveHandle<SurfaceState<R>> | null = opts.machine
        ? null
        : Live<SurfaceState<R>>({ renderer: opts.renderer, view: opts.view, initial: { steps: steps.current } })

    /** The work is done: close the last step and draw the result. */
    function finish(result: R): void {
        steps.move()
        live?.stop({ steps: steps.current, result: result })
    }

    /** The work failed: what to throw. Shown once it's drawn, so it isn't printed again. */
    function fail(thrown: unknown): unknown {
        if (thrown instanceof Shown || !live) return thrown
        const error = errOf(thrown)
        steps.halt()
        live.stop({ steps: steps.current, failure: { error: error } })
        return new Shown(error)
    }

    return {
        move: steps.move,
        advance: steps.advance,
        finish: finish,
        fail: fail,
    }
}

export type SurfaceT<R> = ReturnType<typeof Surface<R>>
