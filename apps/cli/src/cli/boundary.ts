import { errorReport } from "@arcforge/arcline"
import { errOf } from "../errors"
import type { PresentationT } from "./presentation"

/** Boundary — where a command's failure becomes a report and an exit code. */
export function Boundary(opts: { presentation: PresentationT }) {
    /** Run a command: its exit code, or 1 with what went wrong reported. */
    async function run(fn: () => Promise<number>): Promise<number> {
        try {
            return await fn()
        } catch (thrown) {
            const error = errOf(thrown)
            for (const line of errorReport(opts.presentation.renderer, error)) opts.presentation.line(line)
            return 1
        }
    }

    return { run: run }
}

export type BoundaryT = ReturnType<typeof Boundary>
