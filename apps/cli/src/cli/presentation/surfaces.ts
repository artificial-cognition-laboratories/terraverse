import {
    errorReport, header, init as initView, INIT_STEPS, next, prepare as prepareView, publish as publishView, results, status, steps, summarize,
    type RendererHandle, type Result, type TreeNode,
} from "@arcforge/arcline"
import type { PrepareStep, ProjectKind, PublishStep } from "../../terra"
import type { ModeT } from "./mode"
import { Surface, type SurfaceState } from "./surface"

export type InitResult = { files: TreeNode[]; root: string; next: string; ms: number }
export type PublishResult = { version: string; assets: Array<{ path: string; size: string }>; ms: number }
export type PrepareResult = { title: string; modules: Result[]; ms: number }
export type InstallResult = { modules: Result[]; ms: number; next?: string }

/** Prepare's steps as a person reads them, in the order prepare takes them. */
const PREPARING: Record<PrepareStep, string> = {
    install: "Installing",
    config: "Reading config",
    types: "Writing types",
    mods: "Reading mods",
    build: "Building",
}
/** The steps each kind's prepare can take: a mod has no mods of its own to build. */
const PREPARED: Record<ProjectKind, readonly PrepareStep[]> = {
    server: ["install", "config", "types", "mods", "build"],
    mod: ["install", "config", "types"],
}
/** Init's rows: prepare's install is Installing, the rest of it Generating. */
const INITING: Record<PrepareStep, (typeof INIT_STEPS)[number]> = {
    install: "Installing",
    config: "Generating",
    types: "Generating",
    mods: "Generating",
    build: "Generating",
}
const PUBLISHING = ["Preparing", "Verifying", "Packing", "Uploading"] as const
const PUBLISHED: Record<PublishStep, (typeof PUBLISHING)[number]> = { verifying: "Verifying", packing: "Packing", uploading: "Uploading" }

/** Surfaces — each command's live view, built on arcline's: what it shows while working, and when done. */
export function Surfaces(opts: { renderer: RendererHandle; mode: ModeT; registry: { api: string; web: string } }) {
    const surface = <R>(labels: readonly string[], view: (r: RendererHandle, state: SurfaceState<R>, frame: string) => string) =>
        Surface<R>({ renderer: opts.renderer, machine: opts.mode.machine, labels: labels, view: view })

    /** terra <kind> init: the scaffold, its install and its types, then the tree it made and what to run. */
    function init(kind: ProjectKind, name: string) {
        const s = surface<InitResult>(INIT_STEPS, (r, state, frame) => initView(r, { kind: kind, name: name, frame: frame, ...state }))
        return { ...s, scaffolding: () => s.move("Scaffolding"), preparing: (step: PrepareStep) => s.advance(INITING[step]) }
    }

    /** terra publish: prepared, verified, packed and uploaded, then where it went. */
    function publish(name: string) {
        const s = surface<PublishResult>(PUBLISHING, (r, state, frame) => publishView(r, {
            name: name,
            registry: opts.registry,
            steps: state.steps,
            frame: frame,
            ...(state.result ? { result: { ...state.result, visibility: "public" as const } } : {}),
            ...(state.failure ? { failure: state.failure } : {}),
        }))
        return { ...s, preparing: () => s.move("Preparing"), publishing: (step: PublishStep, detail?: string) => s.advance(PUBLISHED[step], detail) }
    }

    /** terra prepare, and every command that prepares first: its steps, collapsing to one line. */
    function prepare(project: string, kind: ProjectKind) {
        const s = surface<PrepareResult>(PREPARED[kind].map(step => PREPARING[step]), (r, state, frame) => prepareView(r, {
            // its folder while it works, and its title once prepare has read it
            project: state.result?.title ?? project,
            steps: state.steps,
            frame: frame,
            ...(state.result ? { modules: state.result.modules, result: { ms: state.result.ms } } : {}),
            ...(state.failure ? { failure: state.failure } : {}),
        }))
        return { ...s, preparing: (step: PrepareStep) => s.advance(PREPARING[step]) }
    }

    /**
     * terra install: the mods asked for, then the prepare that checks them. Drawn here, not
     * with arcline's install view, which speaks of an agent's modules.
     */
    function install(project: string, mods: readonly string[]) {
        const waiting: Result[] = mods.map(mod => ({ name: mod, outcome: "waiting" }))
        const s = surface<InstallResult>(["Preparing"], (r, state, frame) => {
            const rows = state.result?.modules ?? waiting
            const lines = ["", header(r, { title: "Installing into", subtitle: project }), "", ...results(r, rows, { frame: frame }), "", ...steps(r, state.steps, { frame: frame })]
            if (state.result) {
                const summary = summarize(rows, { verb: "installed", noun: "mod" })
                lines.push("", status(r, summary.ok ? "ok" : "fail", summary.text, `${(state.result.ms / 1000).toFixed(1)}s`))
                if (state.result.next) lines.push("", next(r, state.result.next))
            }
            if (state.failure) lines.push("", ...errorReport(r, state.failure.error), "", status(r, "info", "Nothing was installed", "package.json and bunfig.toml are as they were"))
            return [...lines, ""].join("\n")
        })
        return { ...s, preparing: () => s.move("Preparing") }
    }

    return { init: init, publish: publish, prepare: prepare, install: install }
}

export type SurfacesT = ReturnType<typeof Surfaces>
