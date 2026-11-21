import { writeSync } from "node:fs"
import { Live, Renderer, type LiveHandle, type RendererHandle } from "@arcforge/arcline"

/**
 * Output — where the CLI writes: results on stdout, diagnostics on stderr, and --json as
 * one whole line. A test hands in one capture, and everything goes to it.
 */
export function Output(opts: { capture?: RendererHandle }) {
    const renderer = opts.capture ?? Renderer()
    const diagnostics = opts.capture ?? Renderer({ stream: process.stderr })

    /**
     * One line of JSON, written whole. A pipe takes 64 KB at once, and console.log leaves the
     * rest to an event loop the process may exit before draining: the reader would get a
     * cut-off object. writeSync says how much the pipe took, so this loops until it's all gone.
     */
    function emit(text: string): void {
        if (opts.capture) return opts.capture.line(text)
        const bytes = Buffer.from(text + "\n", "utf-8")
        let written = 0
        while (written < bytes.length) {
            try {
                written += writeSync(1, bytes, written, bytes.length - written)
            } catch (cause) {
                if ((cause as { code?: string }).code !== "EAGAIN") throw cause
            }
        }
    }

    /** A line repainted while work runs, where the terminal allows it; in a pipe, nothing. */
    function live<S>(view: (r: RendererHandle, state: S, frame: string) => string, initial: S): Pick<LiveHandle<S>, "set" | "abandon"> {
        if (!renderer.interactive) return { set: () => {}, abandon: () => {} }
        return Live({ renderer: renderer, view: view, initial: initial })
    }

    return {
        renderer: renderer,
        diagnostics: diagnostics,
        line: renderer.line,
        emit: emit,
        live: live,
    }
}

export type OutputT = ReturnType<typeof Output>
