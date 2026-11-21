import type { Step } from "@arcforge/arcline"

/**
 * Steps — the step machine every progress surface drives: which step is running, since
 * when, and what becomes of the one before it. A running step carries `since`, not a
 * frozen `ms`, so the view recomputes its time on every repaint and the counter counts.
 */
export function Steps(opts: { labels: readonly string[]; onChange: (steps: Step[]) => void }) {
    let steps: Step[] = opts.labels.map(label => ({ label: label, state: "waiting" as const }))
    const startedAt = new Map<string, number>()

    /**
     * Close whatever is running and open `to` (nothing, to just close). Re-entering the
     * step already running keeps its start, so a step reported twice accumulates.
     */
    function move(to?: string, detail?: string): void {
        const now = performance.now()
        steps = steps.map(step => {
            if (step.state === "active" && step.label !== to)
                return { ...step, state: "done" as const, ms: now - (startedAt.get(step.label) ?? now), since: undefined }
            if (step.label === to) {
                if (step.state !== "active") startedAt.set(step.label, now)
                return { ...step, state: "active" as const, since: startedAt.get(step.label) ?? now, ...(detail ? { detail: detail } : {}) }
            }
            return step
        })
        opts.onChange(steps)
    }

    /**
     * Move to `to`, closing every step before it too. A step the work never reported is
     * marked skipped, with no time: a duration for something never seen would be made up.
     */
    function advance(to: string, detail?: string): void {
        const target = steps.findIndex(step => step.label === to)
        if (target === -1) throw new Error(`no step ${JSON.stringify(to)} among ${steps.map(s => s.label).join(", ")}`)
        steps = steps.map((step, i) => (i < target && step.state === "waiting" ? { ...step, state: "done" as const, detail: "skipped" } : step))
        move(to, detail)
    }

    /** Mark whatever is running as failed, so the list shows where it died. */
    function halt(): void {
        const now = performance.now()
        steps = steps.map(step =>
            step.state === "active" ? { ...step, state: "failed" as const, ms: now - (startedAt.get(step.label) ?? now), since: undefined } : step,
        )
        opts.onChange(steps)
    }

    return {
        get current(): Step[] {
            return steps
        },
        move: move,
        advance: advance,
        halt: halt,
    }
}

export type StepsT = ReturnType<typeof Steps>
