import type { RendererHandle } from "@arcforge/arcline"
import { Mode } from "./mode"
import { Output } from "./output"
import { Surfaces } from "./surfaces"
import { Tree } from "./tree"

/** Presentation — where the CLI's output goes, who's reading it, and each command's live view. */
export function Presentation(opts: { capture?: RendererHandle; registry: { api: string; web: string } }) {
    const output = Output({ capture: opts.capture })
    const mode = Mode()
    const tree = Tree()
    const surfaces = Surfaces({ renderer: output.renderer, mode: mode, registry: opts.registry })

    return {
        renderer: output.renderer,
        diagnostics: output.diagnostics,
        line: output.line,
        emit: output.emit,
        live: output.live,
        mode: mode,
        tree: tree,
        surfaces: surfaces,
    }
}

export type PresentationT = ReturnType<typeof Presentation>
