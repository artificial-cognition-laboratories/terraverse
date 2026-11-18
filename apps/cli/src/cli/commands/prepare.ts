import { header, results, status, steps, timer, type RendererHandle, type Step } from "@arcforge/arcline"
import type { Prepared, PrepareInput, PrepareStep, TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

const LABELS: Record<PrepareStep, string> = {
    install: "Installing",
    config: "Reading config",
    types: "Writing types",
    mods: "Reading mods",
    build: "Building",
}

/**
 * terra prepare [folder] — the nearest project made ready, whatever its kind. Its steps
 * while it works, then one line, and for a server the mods it runs.
 */
export function PrepareCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        flags.only(args, [])
        await prepare(opts.terra.projects.find(args.positional[0] ?? "."))
        return 0
    }

    /** Prepare a project, showing it. For any command that needs one prepared. */
    async function prepare<P extends Prepared>(project: { root: string; prepare(e?: PrepareInput): Promise<P> }): Promise<P> {
        const started = timer.start()
        const shown: Step[] = []
        const live = p.live((r: RendererHandle, state: { steps: Step[] }, frame: string) =>
            ["", header(r, { title: "Preparing", subtitle: project.root }), "", ...steps(r, state.steps, { frame: frame }), ""].join("\n"),
        { steps: shown })
        try {
            const prepared = await project.prepare({
                progress: step => {
                    const now = performance.now()
                    for (const s of shown) if (s.state === "active") Object.assign(s, { state: "done", ms: Math.round(now - (s.since ?? now)) })
                    shown.push({ label: LABELS[step], state: "active", since: now })
                    live.set({ steps: [...shown] })
                },
            })
            live.abandon()
            summary(prepared, timer.end(started))
            return prepared
        } catch (error) {
            live.abandon()
            throw error
        }
    }

    function summary(prepared: Prepared, ms: number): void {
        const mods = prepared.kind === "server" ? prepared.mods : []
        const detail = prepared.kind === "server" ? `${mods.length} mod${mods.length === 1 ? "" : "s"} · ${ms}ms` : `${prepared.kind} · ${ms}ms`
        p.line(status(p.renderer, "ok", `${prepared.title} ready`, detail))
        for (const row of results(p.renderer, mods.map(m => ({ name: m.name, outcome: "noop" as const, detail: m.version, note: m.prefix }))))
            p.line(`  ${row}`)
    }

    return { run: run, prepare: prepare }
}

export type PrepareCommandT = ReturnType<typeof PrepareCommand>
