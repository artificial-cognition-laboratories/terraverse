import { errorReport } from "@arcforge/arcline"
import { errOf } from "../errors"
import { Shown, type PresentationT } from "./presentation"

/** Boundary — where a command's failure becomes a report and an exit code. */
export function Boundary(opts: { presentation: PresentationT }) {
    /** Run a command: its exit code, or 1 with what went wrong reported, on stderr. */
    async function run(fn: () => Promise<number>): Promise<number> {
        try {
            return await fn()
        } catch (thrown) {
            // a surface already drew it, under the steps that got there
            if (thrown instanceof Shown) return 1
            const r = opts.presentation.diagnostics
            for (const line of errorReport(r, errOf(thrown))) r.line(line)
            return 1
        }
    }

    return { run: run }
}

export type BoundaryT = ReturnType<typeof Boundary>
