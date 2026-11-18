import { Live, Renderer, type LiveHandle, type RendererHandle } from "@arcforge/arcline"

/** Presentation — where the CLI's output goes, and the live surfaces drawn on it. */
export function Presentation(opts: { renderer?: RendererHandle } = {}) {
    const renderer = opts.renderer ?? Renderer()

    /**
     * A surface repainted while work runs, where the terminal allows it. In a pipe or a
     * log it draws nothing, and the caller's final lines say what happened.
     */
    function live<S>(view: (r: RendererHandle, state: S, frame: string) => string, initial: S): Pick<LiveHandle<S>, "set" | "abandon"> {
        if (!renderer.interactive) return { set: () => {}, abandon: () => {} }
        return Live({ renderer: renderer, view: view, initial: initial })
    }

    return {
        renderer: renderer,
        line: renderer.line,
        live: live,
    }
}

export type PresentationT = ReturnType<typeof Presentation>
